/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "fatfs_audio_input.hpp"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <variant>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "ff.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "readahead_source.hpp"

#include "audio_events.hpp"
#include "audio_fsm.hpp"
#include "audio_source.hpp"
#include "codec.hpp"
#include "event_queue.hpp"
#include "fatfs_source.hpp"
#include "future_fetcher.hpp"
#include "spi.hpp"
#include "tag_parser.hpp"
#include "tasks.hpp"
#include "track.hpp"
#include "types.hpp"

[[maybe_unused]] static const char* kTag = "SRC";

namespace audio {

FatfsAudioInput::FatfsAudioInput(database::ITagParser& tag_parser,
                                 tasks::WorkerPool& bg_worker)
    : IAudioSource(),
      tag_parser_(tag_parser),
      bg_worker_(bg_worker),
      new_stream_mutex_(),
      new_stream_(),
      has_new_stream_(false) {}

FatfsAudioInput::~FatfsAudioInput() {}

auto FatfsAudioInput::SetPath(std::optional<std::string> path) -> void {
  if (path) {
    SetPath(*path);
  } else {
    SetPath();
  }
}

auto FatfsAudioInput::SetPath(const std::string& path, uint32_t offset)
    -> void {
  std::lock_guard<std::mutex> guard{new_stream_mutex_};
  if (OpenFile(path, offset)) {
    has_new_stream_ = true;
    has_new_stream_.notify_one();
  }
}

auto FatfsAudioInput::SetPath() -> void {
  std::lock_guard<std::mutex> guard{new_stream_mutex_};
  new_stream_.reset();
  has_new_stream_ = true;
  has_new_stream_.notify_one();
}

auto FatfsAudioInput::HasNewStream() -> bool {
  return has_new_stream_;
}

auto FatfsAudioInput::NextStream() -> std::shared_ptr<TaggedStream> {
  while (true) {
    has_new_stream_.wait(false);

    {
      std::lock_guard<std::mutex> guard{new_stream_mutex_};
      if (!has_new_stream_.exchange(false)) {
        // If the new stream went away, then we need to go back to waiting.
        continue;
      }

      if (new_stream_ == nullptr) {
        continue;
      }

      auto stream = new_stream_;
      new_stream_ = nullptr;
      return stream;
    }
  }
}

auto FatfsAudioInput::OpenFile(const std::string& path, uint32_t offset)
    -> bool {
  ESP_LOGI(kTag, "opening file %s", path.c_str());

  auto tags = tag_parser_.ReadAndParseTags(path);
  if (!tags) {
    ESP_LOGE(kTag, "failed to read tags");
    return false;
  }
  if (!tags->title()) {
    tags->title(path);
  }

  auto stream_type = ContainerToStreamType(tags->encoding());
  if (!stream_type.has_value()) {
    ESP_LOGE(kTag, "couldn't match container to stream");
    return false;
  }

  std::unique_ptr<FIL> file = std::make_unique<FIL>();
  FRESULT res;

  {
    auto lock = drivers::acquire_spi();
    res = f_open(file.get(), path.c_str(), FA_READ);
  }

  if (res != FR_OK) {
    ESP_LOGE(kTag, "failed to open file! res: %i", res);
    return false;
  }

  auto source =
      std::make_unique<FatfsSource>(stream_type.value(), std::move(file));
  new_stream_.reset(new TaggedStream(tags, std::move(source), path, offset));
  return true;
}

auto FatfsAudioInput::ContainerToStreamType(database::Container enc)
    -> std::optional<codecs::StreamType> {
  switch (enc) {
    case database::Container::kMp3:
      return codecs::StreamType::kMp3;
    case database::Container::kWav:
      return codecs::StreamType::kWav;
    case database::Container::kOgg:
      return codecs::StreamType::kVorbis;
    case database::Container::kFlac:
      return codecs::StreamType::kFlac;
    case database::Container::kOpus:
      return codecs::StreamType::kOpus;
    case database::Container::kUnsupported:
    default:
      return {};
  }
}

}  // namespace audio
