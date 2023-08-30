/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "app_console.hpp"
#include "file_gatherer.hpp"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "gpios.hpp"
#include "result.hpp"

#include "audio_fsm.hpp"
#include "event_queue.hpp"
#include "storage.hpp"
#include "system_events.hpp"
#include "system_fsm.hpp"
#include "ui_fsm.hpp"

namespace system_fsm {
namespace states {

static const char kTag[] = "IDLE";
static const TickType_t kTicksBeforeSleep = pdMS_TO_TICKS(10000);

static void timer_callback(TimerHandle_t timer) {
  events::System().Dispatch(internal::IdleTimeout{});
}

/*
 * Ensure the storage and database are both available. If either of these fails
 * to open, then we assume it's an issue with the underlying SD card.
 */
void Idle::entry() {
  ESP_LOGI(kTag, "system became idle");
  events::Audio().Dispatch(OnIdle{});
  events::Ui().Dispatch(OnIdle{});

  sIdleTimeout = xTimerCreate("idle_timeout", kTicksBeforeSleep, false, NULL,
                              timer_callback);
  xTimerStart(sIdleTimeout, portMAX_DELAY);
}

void Idle::exit() {
  xTimerStop(sIdleTimeout, portMAX_DELAY);
  xTimerDelete(sIdleTimeout, portMAX_DELAY);
  ESP_LOGI(kTag, "system left idle");
}

void Idle::react(const KeyLockChanged& ev) {
  if (ev.falling) {
    transit<Running>();
  }
}

void Idle::react(const internal::IdleTimeout& ev) {
  if (!IdleCondition()) {
    // Defensively ensure that we didn't miss an idle-ending event.
    transit<Running>();
    return;
  }
  ESP_LOGI(kTag, "system shutting down");

  // FIXME: It would be neater to just free a bunch of our pointers, deinit the
  // other state machines, etc.
  auto touchwheel = sServices->touchwheel();
  if (touchwheel) {
    touchwheel.value()->PowerDown();
  }

  auto& gpios = sServices->gpios();
  // Pull down to turn things off
  gpios.WriteBuffered(drivers::IGpios::Pin::kAmplifierEnable, false);
  gpios.WriteBuffered(drivers::IGpios::Pin::kSdPowerEnable, false);
  gpios.WriteBuffered(drivers::IGpios::Pin::kDisplayEnable, false);

  // Leave up to match the external pullups
  gpios.WriteBuffered(drivers::IGpios::Pin::kSdMuxSwitch, true);
  gpios.WriteBuffered(drivers::IGpios::Pin::kSdMuxDisable, true);

  // Pull down to prevent sourcing current uselessly from input pins.
  gpios.WriteBuffered(drivers::IGpios::Pin::kSdCardDetect, false);
  gpios.WriteBuffered(drivers::IGpios::Pin::kKeyUp, false);
  gpios.WriteBuffered(drivers::IGpios::Pin::kKeyDown, false);

  gpios.Flush();

  // Retry shutting down in case of a transient failure with the SAMD. e.g. i2c
  // timeouts. This guards against a buggy SAMD firmware preventing idle.
  for (;;) {
    sServices->samd().PowerDown();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

}  // namespace states
}  // namespace system_fsm
