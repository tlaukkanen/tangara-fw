#pragma once

#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"

#include "freertos/message_buffer.h"
#include "span.hpp"

namespace audio {

class StreamBuffer {
 public:
  explicit StreamBuffer(std::size_t chunk_size, std::size_t buffer_size);
  ~StreamBuffer();

  auto Handle() -> MessageBufferHandle_t* { return &handle_; }
  auto ReadBuffer() -> cpp::span<std::byte> { return input_chunk_; }
  auto WriteBuffer() -> cpp::span<std::byte> { return output_chunk_; }

  StreamBuffer(const StreamBuffer&) = delete;
  StreamBuffer& operator=(const StreamBuffer&) = delete;

 private:
  std::byte* raw_memory_;
  StaticMessageBuffer_t metadata_;
  MessageBufferHandle_t handle_;

  std::byte* raw_input_chunk_;
  cpp::span<std::byte> input_chunk_;

  std::byte* raw_output_chunk_;
  cpp::span<std::byte> output_chunk_;
};

}  // namespace audio
