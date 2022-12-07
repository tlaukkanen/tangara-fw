#include "chunk.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

#include "cbor.h"

#include "stream_buffer.hpp"
#include "stream_message.hpp"

namespace audio {

auto WriteChunksToStream(StreamBuffer* stream,
                         std::function<size_t(cpp::span<std::byte>)> callback,
                         TickType_t max_wait) -> ChunkWriteResult {
  cpp::span<std::byte> write_buffer = stream->WriteBuffer();
  while (1) {
    // First, write out our chunk header so we know how much space to give to
    // the callback.
    auto header_size = WriteTypeOnlyMessage(TYPE_CHUNK_HEADER, write_buffer);
    if (header_size.has_error()) {
      return CHUNK_ENCODING_ERROR;
    }

    // Now we can ask the callback to fill the remaining space.
    size_t chunk_size = std::invoke(
        callback,
        write_buffer.subspan(header_size.value(),
                             write_buffer.size() - header_size.value()));

    if (chunk_size == 0) {
      // They had nothing for us, so bail out.
      return CHUNK_OUT_OF_DATA;
    }

    // Try to write to the buffer. Note the return type here will be either 0 or
    // header_size + chunk_size, as MessageBuffer doesn't allow partial writes.
    size_t actual_write_size =
        xMessageBufferSend(stream->Handle(), write_buffer.data(),
                           header_size.value() + chunk_size, max_wait);

    if (actual_write_size == 0) {
      // We failed to write in time, so bail out. This is techinically data loss
      // unless the caller wants to go and parse our working buffer, but we
      // assume the caller has a good reason to time us out.
      return CHUNK_WRITE_TIMEOUT;
    }
  }
}

ChunkReader::ChunkReader(StreamBuffer* stream) : stream_(stream) {}

ChunkReader::~ChunkReader() {}

auto ChunkReader::Reset() -> void {
  leftover_bytes_ = 0;
  last_message_size_ = 0;
}

auto ChunkReader::GetLastMessage() -> cpp::span<std::byte> {
  return stream_->ReadBuffer().subspan(leftover_bytes_, last_message_size_);
}

auto ChunkReader::ReadChunkFromStream(
    std::function<std::optional<size_t>(cpp::span<std::byte>)> callback,
    TickType_t max_wait) -> ChunkReadResult {
  // First, wait for a message to arrive over the buffer.
  cpp::span<std::byte> new_data_dest = stream_->ReadBuffer().last(
      stream_->ReadBuffer().size() - leftover_bytes_);
  last_message_size_ = xMessageBufferReceive(
      stream_->Handle(), new_data_dest.data(), new_data_dest.size(), max_wait);

  if (last_message_size_ == 0) {
    return CHUNK_READ_TIMEOUT;
  }

  cpp::span<std::byte> new_data = GetLastMessage();
  MessageType type = ReadMessageType(new_data);

  if (type != TYPE_CHUNK_HEADER) {
    // This message wasn't for us, so let the caller handle it.
    Reset();
    return CHUNK_STREAM_ENDED;
  }

  // Work the size and position of the chunk.
  auto chunk_data = GetAdditionalData(new_data);

  // Now we need to stick the end of the last chunk (if it exists) onto the
  // front of the new chunk. Do it this way around bc we assume the old chunk
  // is shorter, and therefore faster to move.
  cpp::span<std::byte> leftover_data =
      stream_->ReadBuffer().first(leftover_bytes_);
  cpp::span<std::byte> combined_data(chunk_data.data() - leftover_data.size(),
                                     leftover_data.size() + chunk_data.size());
  if (leftover_bytes_ > 0) {
    std::copy_backward(leftover_data.begin(), leftover_data.end(),
                       combined_data.begin());
  }

  // Tell the callback about the new data.
  std::optional<size_t> amount_processed = std::invoke(callback, combined_data);
  if (!amount_processed) {
    return CHUNK_PROCESSING_ERROR;
  }

  // Prepare for the next iteration.
  leftover_bytes_ = combined_data.size() - amount_processed.value();
  if (leftover_bytes_ > 0) {
    std::copy(combined_data.begin() + amount_processed.value(),
              combined_data.end(), stream_->ReadBuffer().begin());
    return CHUNK_LEFTOVER_DATA;
  }

  return CHUNK_READ_OKAY;
}

}  // namespace audio
