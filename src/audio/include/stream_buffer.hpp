/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"

#include "freertos/message_buffer.h"
#include "span.hpp"

namespace audio {

/*
 * A collection of the buffers required for two IAudioElement implementations to
 * stream data between each other.
 *
 * Currently, we use a FreeRTOS MessageBuffer to hold the byte stream, and also
 * maintain two chunk-sized buffers for the elements to stage their read and
 * write operations (as MessageBuffer copies the given data into its memory
 * space). A future optimisation here could be to instead post himem memory
 * addresses to the message buffer, and then maintain address spaces into which
 * we map these messages, rather than 'real' allocated buffers as we do now.
 */
class StreamBuffer {
 public:
  explicit StreamBuffer(std::size_t chunk_size, std::size_t buffer_size);
  ~StreamBuffer();

  /* Returns the handle for the underlying message buffer. */
  auto Handle() -> MessageBufferHandle_t* { return &handle_; }

  /*
   * Returns a chunk-sized staging buffer that should be used *only* by the
   * reader (sink) element.
   */
  auto ReadBuffer() -> cpp::span<std::byte> { return input_chunk_; }

  /*
   * Returns a chunk-sized staging buffer that should be used *only* by the
   * writer (source) element.
   */
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
