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

  /*
   * Adds samples to the buffer. Returns the number of samples that were added,
   * which may be less than the number of samples given if this PcmBuffer is
   * close to full.
   */
  auto send(std::span<const int16_t>) -> size_t;

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
  auto suspend(bool) -> void;

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
  std::atomic<bool> suspended_;

  RingbufHandle_t ringbuf_;
};

/*
 * Convenience type for a pair of PcmBuffers. Each audio output handles mixing
 * streams together to ensure that low-latency sounds in one channel (e.g. a
 * system notification bleep) aren't delayed by a large audio buffer in the
 * other channel (e.g. a long-running track).
 *
 * By convention, the first buffer of this pair is used for tracks, whilst the
 * second is reserved for 'system sounds'; usually TTS, but potentially maybe
 * other informative noises.
 */
using OutputBuffers = std::pair<PcmBuffer, PcmBuffer>;

}  // namespace drivers
