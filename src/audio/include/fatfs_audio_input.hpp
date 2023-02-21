#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "arena.hpp"
#include "chunk.hpp"
#include "freertos/FreeRTOS.h"

#include "freertos/message_buffer.h"
#include "freertos/queue.h"
#include "span.hpp"

#include "audio_element.hpp"
#include "storage.hpp"
#include "stream_buffer.hpp"

namespace audio {

class FatfsAudioInput : public IAudioElement {
 public:
  explicit FatfsAudioInput(std::shared_ptr<drivers::SdStorage> storage);
  ~FatfsAudioInput();

  auto HasUnprocessedInput() -> bool override;

  auto ProcessStreamInfo(const StreamInfo& info)
      -> cpp::result<void, AudioProcessingError> override;
  auto ProcessChunk(const cpp::span<std::byte>& chunk)
      -> cpp::result<std::size_t, AudioProcessingError> override;
  auto ProcessEndOfStream() -> void override;
  auto Process() -> cpp::result<void, AudioProcessingError> override;

  FatfsAudioInput(const FatfsAudioInput&) = delete;
  FatfsAudioInput& operator=(const FatfsAudioInput&) = delete;

 private:
  memory::Arena arena_;
  std::shared_ptr<drivers::SdStorage> storage_;

  FIL current_file_;
  bool is_file_open_;
};

}  // namespace audio
