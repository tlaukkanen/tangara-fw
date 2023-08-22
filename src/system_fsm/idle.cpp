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
  if (!ev.falling) {
    transit<Running>();
  }
}

void Idle::react(const internal::IdleTimeout& ev) {
  ESP_LOGI(kTag, "system shutting down");

  // FIXME: It would be neater to just free a bunch of our pointers, deinit the
  // other state machines, etc.
  if (sTouch) {
    sTouch->PowerDown();
  }

  // Pull down to turn things off
  sGpios->WriteBuffered(drivers::IGpios::Pin::kAmplifierEnable, false);
  sGpios->WriteBuffered(drivers::IGpios::Pin::kSdPowerEnable, false);
  sGpios->WriteBuffered(drivers::IGpios::Pin::kDisplayEnable, false);

  // Leave up to match the external pullups
  sGpios->WriteBuffered(drivers::IGpios::Pin::kSdMuxSwitch, true);
  sGpios->WriteBuffered(drivers::IGpios::Pin::kSdMuxDisable, true);


  // Pull down to prevent sourcing current uselessly from input pins.
  sGpios->WriteBuffered(drivers::IGpios::Pin::kSdCardDetect, false);
  sGpios->WriteBuffered(drivers::IGpios::Pin::kKeyUp, false);
  sGpios->WriteBuffered(drivers::IGpios::Pin::kKeyDown, false);

  sGpios->Flush();

  sSamd->PowerDown();
}

}  // namespace states
}  // namespace system_fsm
