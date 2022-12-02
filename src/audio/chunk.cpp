#include "chunk.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

#include "cbor.h"

#include "stream_message.hpp"

namespace audio {

// TODO: tune.
const std::size_t kMaxChunkSize = 512;

// TODO: tune
static const std::size_t kWorkingBufferSize = kMaxChunkSize * 1.5;

/*
 * The amount of space to allocate for the first chunk's header. After the first
 * chunk, we have a more concrete idea of the header's size and can allocate
 * space for future headers more compactly.
 */
// TODO: measure how big headers tend to be to pick a better value.
static const std::size_t kInitialHeaderSize = 32;

auto WriteChunksToStream(MessageBufferHandle_t* stream,
                         uint8_t* working_buffer,
                         size_t working_buffer_length,
                         std::function<size_t(uint8_t*, size_t)> callback,
                         TickType_t max_wait) -> ChunkWriteResult {
  size_t header_size = kInitialHeaderSize;
  while (1) {
    // First, ask the callback for some data to write.
    size_t chunk_size = callback(working_buffer + header_size,
                                 working_buffer_length - header_size);

    if (chunk_size == 0) {
      // They had nothing for us, so bail out.
      return CHUNK_OUT_OF_DATA;
    }

    // Put together a header.
    cpp::result<size_t, CborError> encoder_res;
    CborEncoder arr;
    WriteMessage(
        TYPE_CHUNK_HEADER,
        [&](CborEncoder& container) {
          cbor_encoder_create_array(&container, &arr, 2);
          cbor_encode_uint(&arr, header_size);
          cbor_encode_uint(&arr, chunk_size);
          cbor_encoder_close_container(&container, &arr);
          return std::nullopt;
        },
        working_buffer, working_buffer_length);

    size_t new_header_size = header_size;
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
    size_t actual_write_size = xMessageBufferSend(
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

ChunkReader::ChunkReader(MessageBufferHandle_t* stream) : stream_(stream) {
  working_buffer_ = static_cast<uint8_t*>(
      heap_caps_malloc(kWorkingBufferSize, MALLOC_CAP_SPIRAM));
};

ChunkReader::~ChunkReader() {
  free(working_buffer_);
}

auto ChunkReader::Reset() -> void {
  leftover_bytes_ = 0;
  last_message_size_ = 0;
}

auto ChunkReader::GetLastMessage() -> std::pair<uint8_t*, size_t> {
  return std::make_pair(working_buffer_ + leftover_bytes_, last_message_size_);
}

auto ChunkReader::ReadChunkFromStream(
    std::function<std::optional<size_t>(uint8_t*, size_t)> callback,
    TickType_t max_wait) -> ChunkReadResult {
  // First, wait for a message to arrive over the buffer.
  last_message_size_ =
      xMessageBufferReceive(*stream_, working_buffer_ + leftover_bytes_,
                            kWorkingBufferSize - leftover_bytes_, max_wait);

  if (last_message_size_ == 0) {
    return CHUNK_READ_TIMEOUT;
  }

  MessageType type =
      ReadMessageType(working_buffer_ + leftover_bytes_, last_message_size_);

  if (type != TYPE_CHUNK_HEADER) {
    // This message wasn't for us, so let the caller handle it.
    Reset();
    return CHUNK_STREAM_ENDED;
  }

  // Work the size and position of the chunk.
  size_t header_length = 0, chunk_length = 0;

  // TODO: chunker header type to encapsulate this?

  // Now we need to stick the end of the last chunk (if it exists) onto the
  // front of the new chunk. Do it this way around bc we assume the old chunk
  // is shorter, and therefore faster to move.
  uint8_t* combined_buffer = working_buffer_ + header_length - leftover_bytes_;
  size_t combined_buffer_size = leftover_bytes_ + chunk_length;
  if (leftover_bytes_ > 0) {
    memmove(combined_buffer, working_buffer_, leftover_bytes_);
  }

  // Tell the callback about the new data.
  std::optional<size_t> amount_processed =
      callback(combined_buffer, combined_buffer_size);
  if (!amount_processed) {
    return CHUNK_PROCESSING_ERROR;
  }

  // Prepare for the next iteration.
  leftover_bytes_ = combined_buffer_size - amount_processed.value();
  if (leftover_bytes_ > 0) {
    memmove(working_buffer_, combined_buffer + amount_processed.value(),
            leftover_bytes_);
    return CHUNK_LEFTOVER_DATA;
  }

  return CHUNK_READ_OKAY;
}

}  // namespace audio
