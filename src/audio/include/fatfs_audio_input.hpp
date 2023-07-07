/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <future>
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
#include "track.hpp"

#include "audio_element.hpp"
#include "stream_buffer.hpp"
#include "stream_info.hpp"
#include "types.hpp"

namespace audio {

class FatfsAudioInput : public IAudioElement {
 public:
  FatfsAudioInput();
  ~FatfsAudioInput();

  auto CurrentFile() -> std::optional<std::string> { return current_path_; }
  auto OpenFile(std::future<std::optional<std::string>>&& path) -> void;
  auto OpenFile(const std::string& path) -> bool;

  auto NeedsToProcess() const -> bool override;

  auto Process(const std::vector<InputStream>& inputs, OutputStream* output)
      -> void override;

  FatfsAudioInput(const FatfsAudioInput&) = delete;
  FatfsAudioInput& operator=(const FatfsAudioInput&) = delete;

 private:
  auto ContainerToStreamType(database::Encoding)
      -> std::optional<codecs::StreamType>;

  std::optional<std::future<std::optional<std::string>>> pending_path_;
  std::optional<std::string> current_path_;
  FIL current_file_;
  bool is_file_open_;
  bool has_prepared_output_;

  std::optional<database::Encoding> current_container_;
  std::optional<StreamInfo::Format> current_format_;
};

}  // namespace audio
