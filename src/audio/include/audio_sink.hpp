#pragma once

#include <stdint.h>
#include "audio_element.hpp"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "stream_info.hpp"
namespace audio {

class IAudioSink {
 private:
  // TODO: tune. at least about 12KiB seems right for mp3
  static const std::size_t kDrainBufferSize = 48 * 1024;
  uint8_t* buffer_;
  StaticStreamBuffer_t* metadata_;
  StreamBufferHandle_t handle_;

 public:
  IAudioSink()
      : buffer_(reinterpret_cast<uint8_t*>(
            heap_caps_malloc(kDrainBufferSize,
                             MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT))),
        metadata_(reinterpret_cast<StaticStreamBuffer_t*>(
            heap_caps_malloc(sizeof(StaticStreamBuffer_t),
                             MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT))),
        handle_(xStreamBufferCreateStatic(kDrainBufferSize,
                                          1,
                                          buffer_,
                                          metadata_)) {}

  virtual ~IAudioSink() {
    vStreamBufferDelete(handle_);
    free(buffer_);
    free(metadata_);
  }

  virtual auto Configure(const StreamInfo::Format& format) -> bool = 0;
  virtual auto Send(const cpp::span<std::byte>& data) -> void = 0;
  virtual auto Log() -> void {}

  auto buffer() -> StreamBufferHandle_t { return handle_; }
};

}  // namespace audio