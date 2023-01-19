#pragma once

#include <memory>
#include "audio_element.hpp"

namespace audio {

class AudioElementHandle {
 public:
  AudioElementHandle(std::unique_ptr<TaskHandle_t> task,
                     std::shared_ptr<IAudioElement> element);
  ~AudioElementHandle();

  auto CurrentState() -> ElementState;

  // TODO: think about this contract. Would it ever make sense to pause and
  // then walk away? Things could keep running for a whole loop if data comes
  // through, so probably not?
  enum PlayPause {
    PLAY,
    PAUSE,
  };
  auto PlayPause(PlayPause state) -> void;
  auto Quit() -> void;

  auto PauseSync() -> void;
  auto QuitSync() -> void;

  AudioElementHandle(const AudioElementHandle&) = delete;
  AudioElementHandle& operator=(const AudioElementHandle&) = delete;

 private:
  std::unique_ptr<TaskHandle_t> task_;
  std::shared_ptr<IAudioElement> element_;

  auto MonitorUtilState(eTaskState desired) -> void;
  auto SetStateAndWakeUp(ElementState state) -> void;
  auto WakeUpTask() -> void;
};

}  // namespace audio
