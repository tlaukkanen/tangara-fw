#pragma once

#include <cstddef>
#include "ff.h"

namespace audio {

enum SampleRate {};
enum BitDepth {};

struct PcmStreamHeader {
  SampleRate sample_rate;
  BitDepth bit_depth;
  bool configure_now;
};

class AudioDecoder {
 public:
  AudioDecoder();
  ~AudioDecoder();

  auto SetSource(RingbufHandle_t& source) -> void;

  enum Status {};
  auto ProcessChunk() -> Status;

  auto GetOutputStream() const -> RingbufHandle_t;

 private:
  RingbufHandle_t input_;
  RingbufHandle_t output_;
};

}  // namespace audio
