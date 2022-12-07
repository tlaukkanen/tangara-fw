#include "stream_buffer.hpp"

namespace audio {

StreamBuffer::StreamBuffer(std::size_t chunk_size, std::size_t buffer_size)
    : raw_memory_(static_cast<std::byte*>(
          heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM))),
      handle_(
          xMessageBufferCreateStatic(buffer_size,
                                     reinterpret_cast<uint8_t*>(raw_memory_),
                                     &metadata_)),
      raw_input_chunk_(static_cast<std::byte*>(
          heap_caps_malloc(chunk_size * 1.5, MALLOC_CAP_SPIRAM))),
      input_chunk_(raw_input_chunk_, chunk_size * 1.5),
      raw_output_chunk_(static_cast<std::byte*>(
          heap_caps_malloc(chunk_size, MALLOC_CAP_SPIRAM))),
      output_chunk_(raw_output_chunk_, chunk_size) {}

StreamBuffer::~StreamBuffer() {
  vMessageBufferDelete(handle_);
  free(raw_memory_);
  free(raw_input_chunk_);
  free(raw_output_chunk_);
}

}  // namespace audio
