/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdint.h>

#include "adc.hpp"
#include "assert.h"
#include "audio_fsm.hpp"
#include "battery.hpp"
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
#include "service_locator.hpp"
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

auto Booting::entry() -> void {
  ESP_LOGI(kTag, "beginning tangara boot");
  sServices.reset(new ServiceLocator());

  ESP_LOGI(kTag, "installing early drivers");
  // I2C and SPI are both always needed. We can't even power down or show an
  // error without these.
  ESP_ERROR_CHECK(drivers::init_spi());
  sServices->gpios(std::unique_ptr<drivers::Gpios>(drivers::Gpios::Create()));

  ESP_LOGI(kTag, "starting ui");
  if (!ui::UiState::InitBootSplash(sServices->gpios())) {
    events::System().Dispatch(FatalError{});
    return;
  }

  ESP_LOGI(kTag, "installing remaining drivers");
  sServices->samd(std::unique_ptr<drivers::Samd>(drivers::Samd::Create()));
  sServices->nvs(
      std::unique_ptr<drivers::NvsStorage>(drivers::NvsStorage::OpenSync()));
  sServices->touchwheel(
      std::unique_ptr<drivers::TouchWheel>{drivers::TouchWheel::Create()});

  auto adc = drivers::AdcBattery::Create();
  sServices->battery(std::make_unique<battery::Battery>(
      sServices->samd(), std::unique_ptr<drivers::AdcBattery>(adc)));

  sServices->track_queue(std::make_unique<audio::TrackQueue>());
  sServices->tag_parser(std::make_unique<database::TagParserImpl>());

  // ESP_LOGI(kTag, "starting bluetooth");
  // sBluetooth.reset(new drivers::Bluetooth(sNvs.get()));
  // sBluetooth->Enable();

  BootComplete ev{.services = sServices};
  events::Audio().Dispatch(ev);
  events::Ui().Dispatch(ev);
  events::System().Dispatch(ev);
}

auto Booting::exit() -> void {
  // TODO(jacqueline): Gate this on something. Debug flag? Flashing mode?
  sAppConsole = new console::AppConsole();
  sAppConsole->sServices = sServices;
  sAppConsole->Launch();
}

auto Booting::react(const BootComplete& ev) -> void {
  ESP_LOGI(kTag, "bootup completely successfully");

  if (sServices->gpios().Get(drivers::Gpios::Pin::kKeyLock)) {
    transit<Running>();
  } else {
    transit<Idle>();
  }
}

}  // namespace states
}  // namespace system_fsm
