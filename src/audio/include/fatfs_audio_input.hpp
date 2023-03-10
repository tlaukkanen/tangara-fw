#pragma once

#include <cstdint>
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

#include "audio_element.hpp"
#include "stream_buffer.hpp"

namespace audio {

class FatfsAudioInput : public IAudioElement {
 public:
  explicit FatfsAudioInput();
  ~FatfsAudioInput();

  auto OpenFile(const std::string& path) -> void;

  auto Process(std::vector<Stream>* inputs, MutableStream* output)
      -> void override;

  FatfsAudioInput(const FatfsAudioInput&) = delete;
  FatfsAudioInput& operator=(const FatfsAudioInput&) = delete;

 private:
  FIL current_file_;
  bool is_file_open_;
};

}  // namespace audio
