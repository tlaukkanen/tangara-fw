/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include "i2c.hpp"
#include "system_events.hpp"
#include "tinyfsm.hpp"

#include "audio_fsm.hpp"
#include "event_queue.hpp"
#include "system_fsm.hpp"
#include "ui_fsm.hpp"

extern "C" void app_main(void) {
  ESP_ERROR_CHECK(drivers::init_i2c());

  tinyfsm::FsmList<system_fsm::SystemState, ui::UiState,
                   audio::AudioState>::start();

  auto* event_queue = events::queues::SystemAndAudio();
  while (1) {
    event_queue->Service(portMAX_DELAY);
  }
}
