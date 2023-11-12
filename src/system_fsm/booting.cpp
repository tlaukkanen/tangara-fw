/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "collation.hpp"
#include "haptics.hpp"
#include "spiffs.hpp"
#include "system_fsm.hpp"

#include <stdint.h>
#include <memory>

#include "assert.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/timers.h"

#include "adc.hpp"
#include "audio_fsm.hpp"
#include "battery.hpp"
#include "bluetooth.hpp"
#include "bluetooth_types.hpp"
#include "display_init.hpp"
#include "event_queue.hpp"
#include "gpios.hpp"
#include "i2c.hpp"
#include "nvs.hpp"
#include "samd.hpp"
#include "service_locator.hpp"
#include "spi.hpp"
#include "system_events.hpp"
#include "tag_parser.hpp"
#include "tasks.hpp"
#include "touchwheel.hpp"
#include "track_queue.hpp"
#include "ui_fsm.hpp"

namespace system_fsm {
namespace states {

[[maybe_unused]] static const char kTag[] = "BOOT";

static auto bt_event_cb(drivers::bluetooth::Event ev) -> void {
  if (ev == drivers::bluetooth::Event::kKnownDevicesChanged) {
    events::Ui().Dispatch(BluetoothDevicesChanged{});
  }
}

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

  ESP_LOGI(kTag, "starting bg worker");
  sServices->bg_worker(std::unique_ptr<tasks::Worker>{
      tasks::Worker::Start<tasks::Type::kBackgroundWorker>()});

  ESP_LOGI(kTag, "installing remaining drivers");
  drivers::spiffs_mount();
  sServices->samd(std::unique_ptr<drivers::Samd>(drivers::Samd::Create()));
  sServices->nvs(
      std::unique_ptr<drivers::NvsStorage>(drivers::NvsStorage::OpenSync()));
  sServices->touchwheel(
      std::unique_ptr<drivers::TouchWheel>{drivers::TouchWheel::Create()});
  sServices->haptics(std::make_unique<drivers::Haptics>());

  auto adc = drivers::AdcBattery::Create();
  sServices->battery(std::make_unique<battery::Battery>(
      sServices->samd(), std::unique_ptr<drivers::AdcBattery>(adc)));

  sServices->track_queue(std::make_unique<audio::TrackQueue>());
  sServices->tag_parser(std::make_unique<database::TagParserImpl>());
  sServices->collator(locale::CreateCollator());

  ESP_LOGI(kTag, "init bluetooth");
  sServices->bluetooth(std::make_unique<drivers::Bluetooth>(sServices->nvs()));
  sServices->bluetooth().SetEventHandler(bt_event_cb);

  if (sServices->nvs().OutputMode() ==
      drivers::NvsStorage::Output::kBluetooth) {
    ESP_LOGI(kTag, "enabling bluetooth");
    sServices->bluetooth().Enable();
  }

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
    transit<Idle>();
  } else {
    transit<Running>();
  }
}

}  // namespace states
}  // namespace system_fsm
