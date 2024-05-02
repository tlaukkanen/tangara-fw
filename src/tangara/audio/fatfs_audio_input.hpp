/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <string>

#include "ff.h"
#include "freertos/portmacro.h"

#include "audio/audio_source.hpp"
#include "codec.hpp"
#include "database/future_fetcher.hpp"
#include "database/tag_parser.hpp"
#include "tasks.hpp"
#include "types.hpp"

namespace audio {

/*
 * Audio source that fetches data from a FatFs (or exfat i guess) filesystem.
 *
 * All public methods are safe to call from any task.
 */
class FatfsAudioInput : public IAudioSource {
 public:
  explicit FatfsAudioInput(database::ITagParser&, tasks::WorkerPool&);
  ~FatfsAudioInput();

  /*
   * Immediately cease reading any current source, and begin reading from the
   * given file path.
   */
  auto SetPath(std::optional<std::string>) -> void;
  auto SetPath(const std::string&, uint32_t offset = 0) -> void;
  auto SetPath() -> void;

  auto HasNewStream() -> bool override;
  auto NextStream() -> std::shared_ptr<TaggedStream> override;

  FatfsAudioInput(const FatfsAudioInput&) = delete;
  FatfsAudioInput& operator=(const FatfsAudioInput&) = delete;

 private:
  auto OpenFile(const std::string& path, uint32_t offset) -> bool;

  auto ContainerToStreamType(database::Container)
      -> std::optional<codecs::StreamType>;

  database::ITagParser& tag_parser_;
  tasks::WorkerPool& bg_worker_;

  std::mutex new_stream_mutex_;
  std::shared_ptr<TaggedStream> new_stream_;

  std::atomic<bool> has_new_stream_;
};

}  // namespace audio
