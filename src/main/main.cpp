/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include "tinyfsm.hpp"

#include "audio/audio_fsm.hpp"
#include "events/event_queue.hpp"
#include "i2c.hpp"
#include "system_fsm/system_events.hpp"
#include "system_fsm/system_fsm.hpp"
#include "ui/ui_fsm.hpp"

extern "C" void app_main(void) {
  ESP_ERROR_CHECK(drivers::init_i2c());

  tinyfsm::FsmList<system_fsm::SystemState, ui::UiState,
                   audio::AudioState>::start();

  auto* event_queue = events::queues::SystemAndAudio();
  while (1) {
    event_queue->Service(portMAX_DELAY);
  }
}
