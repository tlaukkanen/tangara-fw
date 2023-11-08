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
  SemaphoreHandle_t gpios_semphr = drivers::Gpios::CreateReadPending();
  SemaphoreHandle_t samd_semphr = drivers::Samd::CreateReadPending();

  // Semaphores must be empty before being added to a queue set. Hence all this
  // weird early init stuff; by being explicit about initialisation order, we're
  // able to handle GPIO ISR notifcations + system events from the same task,
  // and a little mess with worth not needing to allocate a whole extra stack.
  QueueSetHandle_t set = xQueueCreateSet(3);
  auto* event_queue = events::queues::SystemAndAudio();
  xQueueAddToSet(event_queue->has_events(), set);
  xQueueAddToSet(gpios_semphr, set);
  xQueueAddToSet(samd_semphr, set);

  tinyfsm::FsmList<system_fsm::SystemState, ui::UiState,
                   audio::AudioState>::start();

  while (1) {
    QueueSetMemberHandle_t member = xQueueSelectFromSet(set, portMAX_DELAY);
    if (member == event_queue->has_events()) {
      event_queue->Service(0);
    } else if (member == gpios_semphr) {
      xSemaphoreTake(member, 0);
      events::System().Dispatch(system_fsm::internal::GpioInterrupt{});
    } else if (member == samd_semphr) {
      xSemaphoreTake(member, 0);
      events::System().Dispatch(system_fsm::internal::SamdInterrupt{});
    }
  }
}
