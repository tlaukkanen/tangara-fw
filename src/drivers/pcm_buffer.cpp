/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "drivers/pcm_buffer.hpp"
#include <stdint.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <span>
#include <tuple>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#include "esp_heap_caps.h"
#include "freertos/ringbuf.h"
#include "portmacro.h"

namespace drivers {

[[maybe_unused]] static const char kTag[] = "pcmbuf";

PcmBuffer::PcmBuffer(size_t size_in_samples) : sent_(0), received_(0) {
  size_t size_in_bytes = size_in_samples * sizeof(int16_t);
  ESP_LOGI(kTag, "allocating pcm buffer of size %u (%uKiB)", size_in_samples,
           size_in_bytes / 1024);
  buf_ = reinterpret_cast<uint8_t*>(
      heap_caps_malloc(size_in_bytes, MALLOC_CAP_SPIRAM));
  ringbuf_ = xRingbufferCreateStatic(size_in_bytes, RINGBUF_TYPE_BYTEBUF, buf_,
                                     &meta_);
}

PcmBuffer::~PcmBuffer() {
  vRingbufferDelete(ringbuf_);
  heap_caps_free(buf_);
}

auto PcmBuffer::send(std::span<const int16_t> data) -> void {
  xRingbufferSend(ringbuf_, data.data(), data.size_bytes(), portMAX_DELAY);
  sent_ += data.size();
}

IRAM_ATTR auto PcmBuffer::receive(std::span<int16_t> dest, bool mix, bool isr)
    -> BaseType_t {
  size_t first_read = 0, second_read = 0;
  BaseType_t ret1 = false, ret2 = false;
  std::tie(first_read, ret1) = readSingle(dest, mix, isr);

  if (first_read < dest.size()) {
    std::tie(second_read, ret2) =
        readSingle(dest.subspan(first_read), mix, isr);
  }

  size_t total_read = first_read + second_read;
  if (total_read < dest.size() && !mix) {
    std::fill_n(dest.begin() + total_read, dest.size() - total_read, 0);
  }

  received_ += first_read + second_read;

  return ret1 || ret2;
}

auto PcmBuffer::clear() -> void {
  while (!isEmpty()) {
    size_t bytes_cleared;
    void* data = xRingbufferReceive(ringbuf_, &bytes_cleared, 0);
    vRingbufferReturnItem(ringbuf_, data);
    received_ += bytes_cleared / sizeof(int16_t);
  }
}

auto PcmBuffer::isEmpty() -> bool {
  return xRingbufferGetMaxItemSize(ringbuf_) ==
         xRingbufferGetCurFreeSize(ringbuf_);
}

auto PcmBuffer::totalSent() -> uint32_t {
  return sent_;
}

auto PcmBuffer::totalReceived() -> uint32_t {
  return received_;
}

IRAM_ATTR auto PcmBuffer::readSingle(std::span<int16_t> dest,
                                     bool mix,
                                     bool isr)
    -> std::pair<size_t, BaseType_t> {
  BaseType_t ret;
  size_t read_bytes = 0;
  void* data;
  if (isr) {
    data =
        xRingbufferReceiveUpToFromISR(ringbuf_, &read_bytes, dest.size_bytes());
  } else {
    data = xRingbufferReceiveUpTo(ringbuf_, &read_bytes, 0, dest.size_bytes());
  }

  size_t read_samples = read_bytes / sizeof(int16_t);

  if (!data) {
    return {read_samples, ret};
  }

  if (mix) {
    for (size_t i = 0; i < read_samples; i++) {
      // Sum the two samples in a 32 bit field so that the addition is always
      // safe.
      int32_t sum = static_cast<int32_t>(dest[i]) +
                    static_cast<int32_t>(reinterpret_cast<int16_t*>(data)[i]);
      // Clip back into the range of a single sample.
      dest[i] = std::clamp<int32_t>(sum, INT16_MIN, INT16_MAX);
    }
  } else {
    std::memcpy(dest.data(), data, read_bytes);
  }

  if (isr) {
    vRingbufferReturnItem(ringbuf_, data);
  } else {
    vRingbufferReturnItemFromISR(ringbuf_, data, &ret);
  }

  return {read_samples, ret};
}

}  // namespace drivers
