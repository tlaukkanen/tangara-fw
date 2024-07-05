/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <atomic>
#include <cstddef>
#include <span>

#include "freertos/FreeRTOS.h"

#include "freertos/ringbuf.h"
#include "portmacro.h"

namespace drivers {

/*
 * A circular buffer of signed, 16-bit PCM samples. PcmBuffers are the main
 * data structure used for shuffling large amounts of read-to-play samples
 * throughout the system.
 */
class PcmBuffer {
 public:
  PcmBuffer(size_t size_in_samples);
  ~PcmBuffer();

  /* Adds samples to the buffer. */
  auto send(std::span<const int16_t>) -> void;

  /*
   * Fills the given span with samples. If enough samples are available in
   * the buffer, then the span will be filled with samples from the buffer. Any
   * shortfall is made up by padding the given span with zeroes.
   *
   * If `mix` is set to true then, instead of overwriting the destination span,
   * the retrieved samples will be mixed into any existing samples contained
   * within the destination. This mixing uses a naive sum approach, and so may
   * introduce clipping.
   */
  auto receive(std::span<int16_t>, bool mix, bool isr) -> BaseType_t;

  auto clear() -> void;
  auto isEmpty() -> bool;

  /*
   * How many samples have been added to this buffer since it was created. This
   * method overflows by wrapping around to zero.
   */
  auto totalSent() -> uint32_t;

  /*
   * How many samples have been removed from this buffer since it was created.
   * This method overflows by wrapping around to zero.
   */
  auto totalReceived() -> uint32_t;

  // Not copyable or movable.
  PcmBuffer(const PcmBuffer&) = delete;
  PcmBuffer& operator=(const PcmBuffer&) = delete;

 private:
  auto readSingle(std::span<int16_t>, bool mix, bool isr)
      -> std::pair<size_t, BaseType_t>;

  StaticRingbuffer_t meta_;
  uint8_t* buf_;

  std::atomic<uint32_t> sent_;
  std::atomic<uint32_t> received_;
  RingbufHandle_t ringbuf_;
};

}  // namespace drivers
