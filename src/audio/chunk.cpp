#include "chunk.hpp"

#include "cbor_encoder.hpp"
#include "cbor_decoder.hpp"
#include <string.h>
#include <cstdint>
#include "esp-idf/components/cbor/tinycbor/src/cbor.h"
#include "stream_message.hpp"

namespace audio {

/*
 * The maximum size that we expect a header to take up.
 */
// TODO: tune this.
static const size_t kMaxHeaderSize = 64;

auto WriteChunksToStream(MessageBufferHandle_t *stream, uint8_t *working_buffer, size_t working_buffer_length, std::function<size_t(uint8_t*,size_t)> callback, TickType_t max_wait) -> EncodeWriteResult {
  while (1) {
    // First, ask the callback for some data to write.
    size_t chunk_size =
      callback(
          working_buffer + kMaxHeaderSize,
          working_buffer_length - kMaxHeaderSize);

    if (chunk_size == 0) {
      // They had nothing for us, so bail out.
      return CHUNK_OUT_OF_DATA;
    }

    // Put together a header.
    cbor::Encoder encoder(cbor::CONTAINER_ARRAY, 3, working_buffer, working_buffer_length);
    encoder.WriteUnsigned(TYPE_CHUNK_HEADER);
    // Note here that we need to write the offset of the chunk into the header.
    // We could be smarter here and write the actual header size, allowing us to
    // pack slightly more data into each message, but this is hard so I haven't
    // done it. Please make my code better for me.
    encoder.WriteUnsigned(kMaxHeaderSize);
    encoder.WriteUnsigned(chunk_size);
    if (encoder.Finish().has_error()) {
      return CHUNK_ENCODING_ERROR;
    };

    // Try to write to the buffer. Note the return type here will be either 0 or
    // kMaxHeaderSize + chunk_size, as MessageBuffer doesn't allow partial
    // writes.
    size_t actual_write_size =
      xMessageBufferSend(
          *stream, working_buffer, kMaxHeaderSize + chunk_size, max_wait);

    if (actual_write_size == 0) {
      // We failed to write in time, so bail out. This is techinically data loss
      // unless the caller wants to go and parse our working buffer, but we
      // assume the caller has a good reason to time us out.
      return CHUNK_WRITE_TIMEOUT;
    }
  }
}

auto ReadChunksFromStream(MessageBufferHandle_t *stream, uint8_t *working_buffer, size_t working_buffer_length, std::function<size_t(uint8_t*,size_t)> callback, TickType_t max_wait) -> EncodeReadResult {
  // Spillover if the previous iteration did not consume all of the input.
  size_t leftover_bytes = 0;
  while (1) {
    // First, wait for a message to arrive over the buffer.
    size_t read_size =
      xMessageBufferReceive(
          *stream, working_buffer + leftover_bytes, working_buffer_length - leftover_bytes, max_wait);

    if (read_size == 0) {
      return CHUNK_READ_TIMEOUT;
    }

    auto decoder = cbor::MapDecoder::Create(working_buffer + leftover_bytes, read_size);
    if (decoder.has_error()) {
      // Weird; this implies someone is shoving invalid data into the buffer.
      return CHUNK_DECODING_ERROR;
    }

    MessageType type = decoder.value().ParseUnsigned().value_or(TYPE_UNKNOWN);
    if (type != TYPE_CHUNK_HEADER) {
      // This message wasn't for us, so put it in a consistent place and let the
      // caller handle it.
      memmove(working_buffer, working_buffer + leftover_bytes, read_size);
      return CHUNK_STREAM_ENDED;
    }

    // Work the size and position of the chunk (don't assume it's at
    // kMaxHeaderSize offset for future-proofing).
    header_length = decoder.ParseUnsigned().value_or(0);
    chunk_length = decoder.ParseUnsigned().value_or(0);
    if (decoder.Failed()) {
      return CHUNK_DECODING_ERROR;
    }

    // Now we need to stick the end of the last chunk (if it exists) onto the
    // front of the new chunk. Do it this way around bc we assume the old chunk
    // is shorter, and therefore faster to move.
    uint8_t *combined_buffer = working_buffer + header_length - leftover_bytes;
    size_t combined_buffer_size = leftover_bytes + chunk_length;
    if (leftover_bytes > 0) {
      memmove(combined_buffer, working_buffer, leftover_bytes);
    }

    // Tell the callback about the new data.
    size_t amount_processed = callback(combined_buffer, combined_buffer_size);

    // Prepare for the next iteration.
    leftover_bytes = combined_buffer_size - amount_processed;
    if (leftover_bytes > 0) {
      memmove(working_buffer, combined_buffer + amount_processed, leftover_bytes);
    }
  }
}

} // namespace audio
