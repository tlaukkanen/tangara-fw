#include "fatfs_audio_input.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

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

auto FatfsAudioInput::OpenFile(const std::string& path) -> void {
  if (is_file_open_) {
    f_close(&current_file_);
    is_file_open_ = false;
  }
  ESP_LOGI(kTag, "opening file %s", path.c_str());
  FRESULT res = f_open(&current_file_, path.c_str(), FA_READ);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "failed to open file! res: %i", res);
    // TODO(jacqueline): Handle errors.
    return;
  }

  is_file_open_ = true;
}

auto FatfsAudioInput::Process(std::vector<Stream>* inputs,
                              MutableStream* output) -> void {
  if (!is_file_open_) {
    return;
  }

  FRESULT result =
      f_read(&current_file_, output->data.data(), output->data.size_bytes(),
             &output->info->bytes_in_stream);
  if (result != FR_OK) {
    ESP_LOGE(kTag, "file I/O error %d", result);
    // TODO(jacqueline): Handle errors.
    return;
  }

  // TODO: read from filename?
  output->info->data = StreamInfo::Encoded{codecs::STREAM_MP3};

  if (output->info->bytes_in_stream < output->data.size_bytes() ||
      f_eof(&current_file_)) {
    f_close(&current_file_);
    is_file_open_ = false;
  }
}

}  // namespace audio
