#pragma once

#include "audio_element.hpp"
#include "stream_info.hpp"
namespace audio {

class IAudioSink {
 private:
  static const std::size_t kDrainBufferSize = 8 * 1024;
  StreamBufferHandle_t buffer_;

 public:
  IAudioSink() : buffer_(xStreamBufferCreate(kDrainBufferSize, 1)) {}
  virtual ~IAudioSink() { vStreamBufferDelete(buffer_); }

  virtual auto Configure(const StreamInfo::Format& format) -> bool = 0;
  virtual auto Send(const cpp::span<std::byte>& data) -> void = 0;

  auto buffer() const -> StreamBufferHandle_t { return buffer_; }
};

}  // namespace audio
