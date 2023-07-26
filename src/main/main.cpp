/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "freertos/portmacro.h"

#include "gpios.hpp"
#include "i2c.hpp"
#include "system_events.hpp"
#include "tinyfsm.hpp"

#include "audio_fsm.hpp"
#include "event_queue.hpp"
#include "system_fsm.hpp"
#include "ui_fsm.hpp"

extern "C" void app_main(void) {
  ESP_ERROR_CHECK(drivers::init_i2c());
  drivers::Gpios* gpios = system_fsm::SystemState::early_init_gpios();

  QueueSetHandle_t set = xQueueCreateSet(2);
  auto* event_queue = events::queues::SystemAndAudio();
  xQueueAddToSet(event_queue->has_events(), set);
  xQueueAddToSet(gpios->IsReadPending(), set);

  tinyfsm::FsmList<system_fsm::SystemState, ui::UiState,
                   audio::AudioState>::start();

  while (1) {
    QueueSetMemberHandle_t member = xQueueSelectFromSet(set, portMAX_DELAY);
    if (member == event_queue->has_events()) {
      event_queue->Service(0);
    } else if (member == gpios->IsReadPending()) {
      xSemaphoreTake(member, 0);
      events::System().Dispatch(system_fsm::internal::GpioInterrupt{});
    }
  }
}
