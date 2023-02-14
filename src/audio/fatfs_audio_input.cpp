#include "fatfs_audio_input.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

#include "esp_heap_caps.h"
#include "freertos/portmacro.h"

#include "audio_element.hpp"
#include "chunk.hpp"
#include "stream_buffer.hpp"
#include "stream_event.hpp"
#include "stream_info.hpp"
#include "stream_message.hpp"

static const char* kTag = "SRC";

namespace audio {

// 32KiB to match the minimum himen region size.
static const std::size_t kChunkSize = 24 * 1024;

FatfsAudioInput::FatfsAudioInput(std::shared_ptr<drivers::SdStorage> storage)
    : IAudioElement(),
      storage_(storage),
      current_file_(),
      is_file_open_(false) {}

FatfsAudioInput::~FatfsAudioInput() {}

auto FatfsAudioInput::HasUnprocessedInput() -> bool {
  return is_file_open_;
}

auto FatfsAudioInput::ProcessStreamInfo(const StreamInfo& info)
    -> cpp::result<void, AudioProcessingError> {
  if (is_file_open_) {
    f_close(&current_file_);
    is_file_open_ = false;
  }

  if (!info.path) {
    return cpp::fail(UNSUPPORTED_STREAM);
  }
  ESP_LOGI(kTag, "opening file %s", info.path->c_str());
  std::string path = *info.path;
  FRESULT res = f_open(&current_file_, path.c_str(), FA_READ);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "failed to open file! res: %i", res);
    return cpp::fail(IO_ERROR);
  }

  is_file_open_ = true;

  StreamInfo new_info(info);
  new_info.chunk_size = kChunkSize;
  ESP_LOGI(kTag, "chunk size: %u bytes", kChunkSize);

  auto event = StreamEvent::CreateStreamInfo(input_events_, new_info);
  SendOrBufferEvent(std::unique_ptr<StreamEvent>(event));

  return {};
}

auto FatfsAudioInput::ProcessChunk(const cpp::span<std::byte>& chunk)
    -> cpp::result<size_t, AudioProcessingError> {
  return cpp::fail(UNSUPPORTED_STREAM);
}

auto FatfsAudioInput::ProcessEndOfStream() -> void {
  if (is_file_open_) {
    f_close(&current_file_);
    is_file_open_ = false;
    SendOrBufferEvent(std::unique_ptr<StreamEvent>(
        StreamEvent::CreateEndOfStream(input_events_)));
  }
}

auto FatfsAudioInput::Process() -> cpp::result<void, AudioProcessingError> {
  if (is_file_open_) {
    auto dest_event = std::unique_ptr<StreamEvent>(
        StreamEvent::CreateChunkData(input_events_, kChunkSize));
    UINT bytes_read = 0;

    FRESULT result = f_read(&current_file_, dest_event->chunk_data.raw_bytes,
                            kChunkSize, &bytes_read);
    if (result != FR_OK) {
      ESP_LOGE(kTag, "file I/O error %d", result);
      return cpp::fail(IO_ERROR);
    }

    dest_event->chunk_data.bytes =
        dest_event->chunk_data.bytes.first(bytes_read);
    SendOrBufferEvent(std::move(dest_event));

    if (bytes_read < kChunkSize || f_eof(&current_file_)) {
      f_close(&current_file_);
      is_file_open_ = false;
    }
  }
  return {};
}

}  // namespace audio
