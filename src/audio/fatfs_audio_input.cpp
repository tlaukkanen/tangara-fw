#include "fatfs_audio_input.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>

#include "arena.hpp"
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

static const std::size_t kChunkSize = 24 * 1024;
static const std::size_t kChunkReadahead = 2;

FatfsAudioInput::FatfsAudioInput(std::shared_ptr<drivers::SdStorage> storage)
    : IAudioElement(),
      arena_(kChunkSize, kChunkReadahead, MALLOC_CAP_SPIRAM),
      storage_(storage),
      current_file_(),
      is_file_open_(false) {}

FatfsAudioInput::~FatfsAudioInput() {}

auto FatfsAudioInput::HasUnprocessedInput() -> bool {
  return is_file_open_;
}

auto FatfsAudioInput::IsOverBuffered() -> bool {
  return arena_.BlocksFree() == 0;
}

auto FatfsAudioInput::ProcessStreamInfo(const StreamInfo& info) -> void {
  if (is_file_open_) {
    f_close(&current_file_);
    is_file_open_ = false;
  }

  if (!info.path) {
    // TODO(jacqueline): Handle errors.
    return;
  }
  ESP_LOGI(kTag, "opening file %s", info.path->c_str());
  std::string path = *info.path;
  FRESULT res = f_open(&current_file_, path.c_str(), FA_READ);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "failed to open file! res: %i", res);
    // TODO(jacqueline): Handle errors.
    return;
  }

  is_file_open_ = true;

  StreamInfo new_info(info);
  new_info.chunk_size = kChunkSize;
  ESP_LOGI(kTag, "chunk size: %u bytes", kChunkSize);

  auto event = StreamEvent::CreateStreamInfo(input_events_, new_info);
  SendOrBufferEvent(std::unique_ptr<StreamEvent>(event));
}

auto FatfsAudioInput::ProcessChunk(const cpp::span<std::byte>& chunk) -> void {}

auto FatfsAudioInput::ProcessEndOfStream() -> void {
  if (is_file_open_) {
    f_close(&current_file_);
    is_file_open_ = false;
    SendOrBufferEvent(std::unique_ptr<StreamEvent>(
        StreamEvent::CreateEndOfStream(input_events_)));
  }
}

auto FatfsAudioInput::Process() -> void {
  if (is_file_open_) {
    auto dest_block = memory::ArenaRef::Acquire(&arena_);
    if (!dest_block) {
      return;
    }

    FRESULT result = f_read(&current_file_, dest_block->ptr.start,
                            dest_block->ptr.size, &dest_block->ptr.used_size);
    if (result != FR_OK) {
      ESP_LOGE(kTag, "file I/O error %d", result);
      // TODO(jacqueline): Handle errors.
      return;
    }

    if (dest_block->ptr.used_size < dest_block->ptr.size ||
        f_eof(&current_file_)) {
      f_close(&current_file_);
      is_file_open_ = false;
    }

    auto dest_event = std::unique_ptr<StreamEvent>(
        StreamEvent::CreateArenaChunk(input_events_, dest_block->Release()));

    SendOrBufferEvent(std::move(dest_event));
  }
}

}  // namespace audio
