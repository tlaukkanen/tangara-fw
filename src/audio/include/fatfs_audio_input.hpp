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

#include "audio_source.hpp"
#include "codec.hpp"
#include "future_fetcher.hpp"
#include "tag_parser.hpp"
#include "types.hpp"

namespace audio {

/*
 * Audio source that fetches data from a FatFs (or exfat i guess) filesystem.
 *
 * All public methods are safe to call from any task.
 */
class FatfsAudioInput : public IAudioSource {
 public:
  explicit FatfsAudioInput(database::ITagParser& tag_parser);
  ~FatfsAudioInput();

  /*
   * Immediately cease reading any current source, and begin reading from the
   * given file path.
   */
  auto SetPath(std::future<std::optional<std::pmr::string>>) -> void;
  auto SetPath(const std::pmr::string&) -> void;
  auto SetPath() -> void;

  auto HasNewStream() -> bool override;
  auto NextStream() -> std::shared_ptr<codecs::IStream> override;

  FatfsAudioInput(const FatfsAudioInput&) = delete;
  FatfsAudioInput& operator=(const FatfsAudioInput&) = delete;

 private:
  auto OpenFile(const std::pmr::string& path) -> bool;

  auto ContainerToStreamType(database::Container)
      -> std::optional<codecs::StreamType>;

  database::ITagParser& tag_parser_;

  std::mutex new_stream_mutex_;
  std::shared_ptr<codecs::IStream> new_stream_;

  std::atomic<bool> has_new_stream_;

  std::unique_ptr<database::FutureFetcher<std::optional<std::pmr::string>>>
      pending_path_;
};

}  // namespace audio
