#include "chunk.hpp"

#include "cbor_encoder.hpp"
#include "cbor_decoder.hpp"
#include <string.h>
#include <cstdint>
#include "esp-idf/components/cbor/tinycbor/src/cbor.h"
#include "stream_message.hpp"

namespace audio {

/*
 * The amount of space to allocate for the first chunk's header. After the first
 * chunk, we have a more concrete idea of the header's size and can allocate
 * space for future headers more compactly.
 */
// TODO: measure how big headers tend to be to pick a better value.
static const size_t kInitialHeaderSize = 32;

auto WriteChunksToStream(MessageBufferHandle_t *stream, uint8_t *working_buffer, size_t working_buffer_length, std::function<size_t(uint8_t*,size_t)> callback, TickType_t max_wait) -> EncodeWriteResult {

  size_t header_size = kInitialHeaderSize;
  while (1) {
    // First, ask the callback for some data to write.
    size_t chunk_size =
      callback(
          working_buffer + header_size,
          working_buffer_length - header_size);

    if (chunk_size == 0) {
      // They had nothing for us, so bail out.
      return CHUNK_OUT_OF_DATA;
    }

    // Put together a header.
    cbor::Encoder encoder(cbor::CONTAINER_ARRAY, 3, working_buffer, working_buffer_length);
    encoder.WriteUnsigned(TYPE_CHUNK_HEADER);
    encoder.WriteUnsigned(header_size);
    encoder.WriteUnsigned(chunk_size);

    size_t new_header_size = header_size;
    cpp::result<size_t, CborError> encoder_res = encoder.Finish();
    if (encoder_res.has_error()) {
      return CHUNK_ENCODING_ERROR;
    } else {
      // We can now tune the space to allocate for the header to be closer to
      // its actual size. We pad this by 2 bytes to allow extra space for the
      // chunk size and header size fields to each spill over into another byte
      // each.
      new_header_size = encoder_res.value() + 2;
    }

    // Try to write to the buffer. Note the return type here will be either 0 or
    // header_size + chunk_size, as MessageBuffer doesn't allow partial writes.
    size_t actual_write_size =
      xMessageBufferSend(
          *stream, working_buffer, header_size + chunk_size, max_wait);

    header_size = new_header_size;

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

    // Work the size and position of the chunk.
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
