/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "system_fsm/system_fsm.hpp"

#include <cstdint>
#include <memory>

#include "assert.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/timers.h"

#include "audio/audio_fsm.hpp"
#include "audio/track_queue.hpp"
#include "battery/battery.hpp"
#include "collation.hpp"
#include "database/tag_parser.hpp"
#include "drivers/adc.hpp"
#include "drivers/bluetooth.hpp"
#include "drivers/bluetooth_types.hpp"
#include "drivers/display_init.hpp"
#include "drivers/gpios.hpp"
#include "drivers/haptics.hpp"
#include "drivers/i2c.hpp"
#include "drivers/nvs.hpp"
#include "drivers/samd.hpp"
#include "drivers/spi.hpp"
#include "drivers/spiffs.hpp"
#include "drivers/touchwheel.hpp"
#include "events/event_queue.hpp"
#include "system_fsm/service_locator.hpp"
#include "system_fsm/system_events.hpp"
#include "tasks.hpp"
#include "tts/provider.hpp"
#include "ui/ui_fsm.hpp"

namespace system_fsm {
namespace states {

[[maybe_unused]] static const char kTag[] = "BOOT";

static auto bt_event_cb(drivers::bluetooth::Event ev) -> void {
  events::Ui().Dispatch(BluetoothEvent{.event = ev});
  events::Audio().Dispatch(BluetoothEvent{.event = ev});
}

static const TickType_t kInterruptCheckPeriod = pdMS_TO_TICKS(100);

auto Booting::entry() -> void {
  ESP_LOGI(kTag, "beginning tangara boot");
  sServices.reset(new ServiceLocator());

  ESP_LOGI(kTag, "installing early drivers");
  // NVS is needed first because it contains information about what specific
  // hardware configuration we're running on.
  sServices->nvs(
      std::unique_ptr<drivers::NvsStorage>(drivers::NvsStorage::OpenSync()));

  // HACK: tell the unit that it has an ERM motor (we will likely default to
  //       LRAs in future, but all the current units in the field use ERMs.)
  // sServices->nvs().HapticMotorIsErm(true);

  // HACK: fix up the switch polarity on newer dev units
  // sServices->nvs().LockPolarity(false);

  // I2C and SPI are both always needed. We can't even power down or show an
  // error without these.
  ESP_ERROR_CHECK(drivers::init_spi());
  sServices->gpios(std::unique_ptr<drivers::Gpios>(
      drivers::Gpios::Create(sServices->nvs().LockPolarity())));

  ESP_LOGI(kTag, "starting ui");
  if (!ui::UiState::InitBootSplash(sServices->gpios(), sServices->nvs())) {
    events::System().Dispatch(FatalError{});
    return;
  }

  ESP_LOGI(kTag, "starting bg worker");
  sServices->bg_worker(std::make_unique<tasks::WorkerPool>());

  ESP_LOGI(kTag, "installing remaining drivers");
  drivers::spiffs_mount();
  sServices->samd(std::unique_ptr<drivers::Samd>(drivers::Samd::Create()));
  sServices->touchwheel(
      std::unique_ptr<drivers::TouchWheel>{drivers::TouchWheel::Create()});
  sServices->haptics(std::make_unique<drivers::Haptics>(sServices->nvs()));

  auto adc = drivers::AdcBattery::Create();
  sServices->battery(std::make_unique<battery::Battery>(
      sServices->samd(), std::unique_ptr<drivers::AdcBattery>(adc)));

  sServices->track_queue(
      std::make_unique<audio::TrackQueue>(sServices->bg_worker()));
  sServices->tag_parser(std::make_unique<database::TagParserImpl>());
  sServices->collator(locale::CreateCollator());
  sServices->tts(std::make_unique<tts::Provider>());

  ESP_LOGI(kTag, "init bluetooth");
  sServices->bluetooth(std::make_unique<drivers::Bluetooth>(
      sServices->nvs(), sServices->bg_worker()));
  sServices->bluetooth().SetEventHandler(bt_event_cb);

  BootComplete ev{.services = sServices};
  events::Audio().Dispatch(ev);
  events::Ui().Dispatch(ev);
  events::System().Dispatch(ev);
}

auto Booting::exit() -> void {
  // TODO(jacqueline): Gate this on something. Debug flag? Flashing mode?
  sServices->bg_worker().Dispatch<void>([&] {
    sAppConsole = new console::AppConsole();
    sAppConsole->sServices = sServices;
    sAppConsole->Launch();
  });

  TimerHandle_t timer = xTimerCreate("INTERRUPTS", kInterruptCheckPeriod, true,
                                     NULL, check_interrupts_cb);
  xTimerStart(timer, portMAX_DELAY);
}

auto Booting::react(const BootComplete& ev) -> void {
  ESP_LOGI(kTag, "bootup completely successfully");

  if (sServices->gpios().IsLocked()) {
    transit<Idle>();
  } else {
    transit<Running>();
  }
}

}  // namespace states
}  // namespace system_fsm
