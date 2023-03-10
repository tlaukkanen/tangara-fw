#pragma once

#include <memory>
#include <optional>
#include <string>

#include "audio_element.hpp"
#include "freertos/portmacro.h"
#include "pipeline.hpp"

namespace audio {

namespace task {
struct AudioTaskArgs {
  Pipeline* pipeline;
  QueueHandle_t input;
};

extern "C" void AudioTaskMain(void* args);

enum Command { PLAY, PAUSE, QUIT };

class Handle {
 public:
  explicit Handle(QueueHandle_t input);
  ~Handle();

  auto SetStreamInfo() -> void;
  auto Play() -> void;
  auto Pause() -> void;
  auto Quit() -> void;

  auto OutputBuffer() -> StreamBufferHandle_t;

 private:
  QueueHandle_t input;
};

auto Start(Pipeline* pipeline) -> Handle*;

}  // namespace task

}  // namespace audio
