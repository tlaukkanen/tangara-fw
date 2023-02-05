#include "chunk.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>

#include "cbor.h"
#include "esp_log.h"

#include "stream_buffer.hpp"
#include "stream_message.hpp"

namespace audio {

static const std::size_t kWorkingBufferMultiple = 2;

ChunkReader::ChunkReader(std::size_t chunk_size)
    : raw_working_buffer_(static_cast<std::byte*>(
          heap_caps_malloc(chunk_size * kWorkingBufferMultiple,
                           MALLOC_CAP_SPIRAM))),
      working_buffer_(raw_working_buffer_,
                      chunk_size * kWorkingBufferMultiple) {}

ChunkReader::~ChunkReader() {
  free(raw_working_buffer_);
}

auto ChunkReader::HandleNewData(cpp::span<std::byte> data)
    -> cpp::span<std::byte> {
  assert(leftover_bytes_ + data.size() <= working_buffer_.size());
  // Copy the new data onto the front for anything that was left over from the
  // last portion. Note: this could be optimised for the '0 leftover bytes'
  // case, which technically shouldn't need a copy.
  std::copy(data.begin(), data.end(),
            working_buffer_.begin() + leftover_bytes_);
  last_data_in_working_buffer_ =
      working_buffer_.first(leftover_bytes_ + data.size());
  leftover_bytes_ = 0;
  return last_data_in_working_buffer_;
}

auto ChunkReader::HandleLeftovers(std::size_t bytes_used) -> void {
  leftover_bytes_ = last_data_in_working_buffer_.size() - bytes_used;

  // Ensure that we don't have more than a chunk of leftever bytes. This is
  // bad, because we probably won't have enough data to store the next chunk.
  assert(leftover_bytes_ <= working_buffer_.size() / kWorkingBufferMultiple);

  if (leftover_bytes_ > 0) {
    auto data_to_keep = last_data_in_working_buffer_.last(leftover_bytes_);
    std::copy(data_to_keep.begin(), data_to_keep.end(),
              working_buffer_.begin());
  }
}

}  // namespace audio
