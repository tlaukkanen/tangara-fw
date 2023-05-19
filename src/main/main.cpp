#include "freertos/portmacro.h"

#include "tinyfsm.hpp"

#include "audio_fsm.hpp"
#include "event_queue.hpp"
#include "system_fsm.hpp"
#include "ui_fsm.hpp"

extern "C" void app_main(void) {
  tinyfsm::FsmList<system_fsm::SystemState, ui::UiState,
                   audio::AudioState>::start();

  auto& queue = events::EventQueue::GetInstance();
  while (1) {
    queue.Service(portMAX_DELAY);
  }
}
