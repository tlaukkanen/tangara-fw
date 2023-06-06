/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "fatfs_audio_input.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <variant>

#include "arena.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "ff.h"
#include "freertos/portmacro.h"

#include "audio_element.hpp"
#include "chunk.hpp"
#include "stream_buffer.hpp"
#include "stream_event.hpp"
#include "stream_info.hpp"
#include "stream_message.hpp"
#include "types.hpp"

static const char* kTag = "SRC";

namespace audio {

FatfsAudioInput::FatfsAudioInput()
    : IAudioElement(), current_file_(), is_file_open_(false) {}

FatfsAudioInput::~FatfsAudioInput() {}

auto FatfsAudioInput::OpenFile(const std::string& path) -> bool {
  if (is_file_open_) {
    f_close(&current_file_);
    is_file_open_ = false;
  }
  ESP_LOGI(kTag, "opening file %s", path.c_str());
  FRESULT res = f_open(&current_file_, path.c_str(), FA_READ);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "failed to open file! res: %i", res);
    return false;
  }

  is_file_open_ = true;
  return true;
}

auto FatfsAudioInput::NeedsToProcess() const -> bool {
  return is_file_open_;
}

auto FatfsAudioInput::Process(const std::vector<InputStream>& inputs,
                              OutputStream* output) -> void {
  if (!is_file_open_) {
    // TODO(jacqueline): should we clear the stream format?
    // output->prepare({});
    return;
  }

  StreamInfo::Format format = StreamInfo::Encoded{codecs::STREAM_MP3};
  if (!output->prepare(format)) {
    return;
  }

  std::size_t max_size = output->data().size_bytes();
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
    f_close(&current_file_);
    is_file_open_ = false;
  }
}

}  // namespace audio
