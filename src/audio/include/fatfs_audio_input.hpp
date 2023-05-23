/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "arena.hpp"
#include "chunk.hpp"
#include "freertos/FreeRTOS.h"

#include "ff.h"
#include "freertos/message_buffer.h"
#include "freertos/queue.h"
#include "span.hpp"

#include "audio_element.hpp"
#include "stream_buffer.hpp"
#include "stream_info.hpp"

namespace audio {

class FatfsAudioInput : public IAudioElement {
 public:
  explicit FatfsAudioInput();
  ~FatfsAudioInput();

  auto OpenFile(const std::string& path) -> void;

  auto Process(const std::vector<InputStream>& inputs, OutputStream* output)
      -> void override;

  FatfsAudioInput(const FatfsAudioInput&) = delete;
  FatfsAudioInput& operator=(const FatfsAudioInput&) = delete;

 private:
  FIL current_file_;
  bool is_file_open_;
};

}  // namespace audio
