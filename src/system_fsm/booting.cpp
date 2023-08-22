/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

#include "assert.h"
#include "audio_fsm.hpp"
#include "bluetooth.hpp"
#include "core/lv_obj.h"
#include "display_init.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "event_queue.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/timers.h"
#include "gpios.hpp"
#include "lvgl/lvgl.h"
#include "nvs.hpp"
#include "relative_wheel.hpp"
#include "samd.hpp"
#include "spi.hpp"
#include "system_events.hpp"
#include "system_fsm.hpp"
#include "tag_parser.hpp"
#include "track_queue.hpp"
#include "ui_fsm.hpp"

#include "i2c.hpp"
#include "touchwheel.hpp"

namespace system_fsm {
namespace states {

static const char kTag[] = "BOOT";

static const TickType_t kBatteryCheckPeriod = pdMS_TO_TICKS(60 * 1000);

static void battery_timer_cb(TimerHandle_t timer) {
  events::System().Dispatch(internal::BatteryTimerFired{});
}

auto Booting::entry() -> void {
  ESP_LOGI(kTag, "beginning tangara boot");
  ESP_LOGI(kTag, "installing early drivers");

  // I2C and SPI are both always needed. We can't even power down or show an
  // error without these.
  ESP_ERROR_CHECK(drivers::init_spi());
  sGpios.reset(drivers::Gpios::Create());

  // Start bringing up LVGL now, since we have all of its prerequisites.
  sTrackQueue.reset(new audio::TrackQueue());
  ESP_LOGI(kTag, "starting ui");
  if (!ui::UiState::Init(sGpios.get(), sTrackQueue.get())) {
    events::System().Dispatch(FatalError{});
    return;
  }

  // Install everything else that is certain to be needed.
  ESP_LOGI(kTag, "installing remaining drivers");
  sSamd.reset(drivers::Samd::Create());
  sBattery.reset(drivers::Battery::Create());
  sNvs.reset(drivers::NvsStorage::Open());
  sTagParser.reset(new database::TagParserImpl());

  if (!sSamd || !sBattery || !sNvs) {
    events::System().Dispatch(FatalError{});
    events::Ui().Dispatch(FatalError{});
    return;
  }

  ESP_LOGI(kTag, "battery is at %u%% (= %lu mV)", sBattery->Percent(),
           sBattery->Millivolts());
  TimerHandle_t battery_timer = xTimerCreate("battery", kBatteryCheckPeriod,
                                             true, NULL, battery_timer_cb);
  xTimerStart(battery_timer, portMAX_DELAY);

  ESP_LOGI(kTag, "starting bluetooth");
  sBluetooth.reset(new drivers::Bluetooth(sNvs.get()));
  // sBluetooth->Enable();

  // At this point we've done all of the essential boot tasks. Start remaining
  // state machines and inform them that the system is ready.

  ESP_LOGI(kTag, "starting audio");
  if (!audio::AudioState::Init(sGpios.get(), sDatabase, sTagParser,
                               sBluetooth.get(), sTrackQueue.get())) {
    events::System().Dispatch(FatalError{});
    events::Ui().Dispatch(FatalError{});
    return;
  }

  events::System().Dispatch(BootComplete{});
  events::Audio().Dispatch(BootComplete{});
  events::Ui().Dispatch(BootComplete{});
}

auto Booting::exit() -> void {
  // TODO(jacqueline): Gate this on something. Debug flag? Flashing mode?
  sAppConsole = new console::AppConsole();
  sAppConsole->sTrackQueue = sTrackQueue.get();
  sAppConsole->sBluetooth = sBluetooth.get();
  sAppConsole->sSamd = sSamd.get();
  sAppConsole->Launch();
}

auto Booting::react(const BootComplete& ev) -> void {
  ESP_LOGI(kTag, "bootup completely successfully");

  if (sGpios->Get(drivers::Gpios::Pin::kKeyLock)) {
    transit<Running>();
  } else {
    transit<Idle>();
  }
}

}  // namespace states
}  // namespace system_fsm
