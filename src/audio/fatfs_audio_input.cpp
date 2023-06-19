/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "fatfs_audio_input.hpp"
#include <stdint.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <variant>

#include "arena.hpp"
#include "audio_events.hpp"
#include "audio_fsm.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "event_queue.hpp"
#include "ff.h"
#include "freertos/portmacro.h"

#include "audio_element.hpp"
#include "chunk.hpp"
#include "stream_buffer.hpp"
#include "stream_event.hpp"
#include "stream_info.hpp"
#include "stream_message.hpp"
#include "tag_parser.hpp"
#include "track.hpp"
#include "types.hpp"

static const char* kTag = "SRC";

namespace audio {

FatfsAudioInput::FatfsAudioInput()
    : IAudioElement(),
      pending_path_(),
      current_file_(),
      is_file_open_(false),
      has_prepared_output_(false),
      current_container_(),
      current_format_() {}

FatfsAudioInput::~FatfsAudioInput() {}

auto FatfsAudioInput::OpenFile(std::future<std::optional<std::string>>&& path)
    -> void {
  pending_path_ = std::move(path);
}

auto FatfsAudioInput::OpenFile(const std::string& path) -> bool {
  if (is_file_open_) {
    f_close(&current_file_);
    is_file_open_ = false;
    has_prepared_output_ = false;
  }

  if (pending_path_) {
    pending_path_ = {};
  }

  ESP_LOGI(kTag, "opening file %s", path.c_str());

  database::TagParserImpl tag_parser;
  database::TrackTags tags;
  if (!tag_parser.ReadAndParseTags(path, &tags)) {
    ESP_LOGE(kTag, "failed to read tags");
    tags.encoding = database::Encoding::kFlac;
    // return false;
  }

  auto stream_type = ContainerToStreamType(tags.encoding);
  if (!stream_type.has_value()) {
    ESP_LOGE(kTag, "couldn't match container to stream");
    return false;
  }

  current_container_ = tags.encoding;

  if (*stream_type == codecs::StreamType::kPcm && tags.channels &&
      tags.bits_per_sample && tags.channels) {
    // WAV files are a special case bc they contain raw PCM streams. These don't
    // need decoding, but we *do* need to parse the PCM format from the header.
    // TODO(jacqueline): Maybe we should have a decoder for this just to deal
    // with endianness differences?
    current_format_ = StreamInfo::Pcm{
        .channels = static_cast<uint8_t>(*tags.channels),
        .bits_per_sample = static_cast<uint8_t>(*tags.bits_per_sample),
        .sample_rate = static_cast<uint32_t>(*tags.sample_rate),
    };
  } else {
    current_format_ = StreamInfo::Encoded{*stream_type};
  }

  FRESULT res = f_open(&current_file_, path.c_str(), FA_READ);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "failed to open file! res: %i", res);
    return false;
  }

  events::Dispatch<InputFileOpened, AudioState>({});
  is_file_open_ = true;
  return true;
}

auto FatfsAudioInput::NeedsToProcess() const -> bool {
  return is_file_open_ || pending_path_;
}

auto FatfsAudioInput::Process(const std::vector<InputStream>& inputs,
                              OutputStream* output) -> void {
  if (pending_path_) {
    if (!pending_path_->valid()) {
      pending_path_ = {};
    } else {
      if (pending_path_->wait_for(std::chrono::seconds(0)) ==
          std::future_status::ready) {
        auto result = pending_path_->get();
        if (result) {
          OpenFile(*result);
        }
      }
    }
  }

  if (!is_file_open_) {
    return;
  }

  if (!has_prepared_output_ && !output->prepare(*current_format_)) {
    ESP_LOGI(kTag, "waiting for buffer to free up");
    return;
  }
  has_prepared_output_ = true;

  std::size_t max_size = output->data().size_bytes();
  if (max_size < output->data().size_bytes() / 2) {
    return;
  }

  std::size_t size = 0;
  FRESULT result =
      f_read(&current_file_, output->data().data(), max_size, &size);
  if (result != FR_OK) {
    ESP_LOGE(kTag, "file I/O error %d", result);
    // TODO(jacqueline): Handle errors.
    return;
  }

  output->add(size);

  if (size < max_size || f_eof(&current_file_)) {
    ESP_LOGI(kTag, "file finished. closing.");
    f_close(&current_file_);
    is_file_open_ = false;
    has_prepared_output_ = false;
    output->mark_producer_finished();

    events::Dispatch<InputFileFinished, AudioState>({});
  }
}

auto FatfsAudioInput::ContainerToStreamType(database::Encoding enc)
    -> std::optional<codecs::StreamType> {
  switch (enc) {
    case database::Encoding::kMp3:
      return codecs::StreamType::kMp3;
    case database::Encoding::kWav:
      return codecs::StreamType::kPcm;
    case database::Encoding::kFlac:
      return codecs::StreamType::kFlac;
    case database::Encoding::kOgg:  // Misnamed; this is Ogg Vorbis.
      return codecs::StreamType::kVorbis;
    case database::Encoding::kUnsupported:
    default:
      return {};
  }
}

}  // namespace audio
