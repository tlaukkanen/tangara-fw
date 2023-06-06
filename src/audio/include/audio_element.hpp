/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <vector>

#include "freertos/FreeRTOS.h"

#include "chunk.hpp"
#include "freertos/message_buffer.h"
#include "freertos/portmacro.h"
#include "result.hpp"
#include "span.hpp"

#include "stream_buffer.hpp"
#include "stream_event.hpp"
#include "stream_info.hpp"
#include "types.hpp"

namespace audio {

static const size_t kEventQueueSize = 8;

/*
 * One indepedentent part of an audio pipeline. Each element has an input and
 * output message stream, and is responsible for taking data from the input
 * stream, applying some kind of transformation to it, then sending the result
 * out via the output stream. All communication with an element should be done
 * over these streams, as an element's methods are only safe to call from the
 * task that owns it.
 *
 * Each element implementation will have its input stream automatically parsed
 * by its owning task, and its various Process* methods will be invoked
 * accordingly. Element implementations are responsible for managing their own
 * writes to their output streams.
 */
class IAudioElement {
 public:
  IAudioElement() {}
  virtual ~IAudioElement() {}

  virtual auto NeedsToProcess() const -> bool = 0;

  virtual auto Process(const std::vector<InputStream>& inputs,
                       OutputStream* output) -> void = 0;
};

}  // namespace audio
