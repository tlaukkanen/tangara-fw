#include "audio_element_handle.hpp"
#include "audio_element.hpp"
#include "freertos/projdefs.h"
#include "freertos/task.h"

namespace audio {

AudioElementHandle::AudioElementHandle(std::unique_ptr<TaskHandle_t> task,
                                       std::shared_ptr<IAudioElement> element)
    : task_(std::move(task)), element_(std::move(element)) {}

AudioElementHandle::~AudioElementHandle() {
  Quit();
}

auto AudioElementHandle::CurrentState() -> ElementState {
  return element_->ElementState();
}

auto AudioElementHandle::PlayPause(enum PlayPause state) -> void {
  ElementState s = CurrentState();
  if (state == PLAY && s == STATE_PAUSE) {
    // Ensure we actually finished any previous pause command.
    // TODO: really?
    PauseSync();
    SetStateAndWakeUp(STATE_RUN);
    return;
  }
  if (state == PAUSE && s == STATE_RUN) {
    element_->ElementState(STATE_PAUSE);
    SetStateAndWakeUp(STATE_PAUSE);
    return;
  }
}

auto AudioElementHandle::Quit() -> void {
  SetStateAndWakeUp(STATE_QUIT);
}

auto AudioElementHandle::PauseSync() -> void {
  PlayPause(PAUSE);
  MonitorUtilState(eSuspended);
}

auto AudioElementHandle::QuitSync() -> void {
  Quit();
  MonitorUtilState(eDeleted);
}

auto AudioElementHandle::MonitorUtilState(eTaskState desired) -> void {
  while (eTaskGetState(*task_) != desired) {
    WakeUpTask();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

auto AudioElementHandle::SetStateAndWakeUp(ElementState state) -> void {
  element_->ElementState(state);
  WakeUpTask();
}

auto AudioElementHandle::WakeUpTask() -> void {
  // TODO: various races where the task isn't blocked yet, but there is a block
  // between now and its next element state check. Also think about chunk blocks
  // nested in element bodies.
  // Maybe we need a big mutex or semaphore somewhere in here.
  switch (eTaskGetState(*task_)) {
    case eBlocked:
      // TODO: when is this safe?
      xTaskAbortDelay(*task_);
      break;
    case eSuspended:
      vTaskResume(*task_);
      break;
    default:
      return;
  }
}

}  // namespace audio
