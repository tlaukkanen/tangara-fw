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
#include "song.hpp"
#include "span.hpp"

#include "audio_element.hpp"
#include "stream_buffer.hpp"
#include "stream_info.hpp"
#include "types.hpp"

namespace audio {

class FatfsAudioInput : public IAudioElement {
 public:
  FatfsAudioInput();
  ~FatfsAudioInput();

  auto OpenFile(const std::string& path) -> bool;

  auto NeedsToProcess() const -> bool override;

  auto Process(const std::vector<InputStream>& inputs, OutputStream* output)
      -> void override;

  FatfsAudioInput(const FatfsAudioInput&) = delete;
  FatfsAudioInput& operator=(const FatfsAudioInput&) = delete;

 private:
  auto ContainerToStreamType(database::Encoding)
      -> std::optional<codecs::StreamType>;

  FIL current_file_;
  bool is_file_open_;

  std::optional<database::Encoding> current_container_;
  std::optional<StreamInfo::Format> current_format_;
};

}  // namespace audio
