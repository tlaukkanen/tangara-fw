/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "assert.h"
#include "audio_fsm.hpp"
#include "core/lv_obj.h"
#include "display_init.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "event_queue.hpp"
#include "gpio_expander.hpp"
#include "lvgl/lvgl.h"
#include "relative_wheel.hpp"
#include "spi.hpp"
#include "system_events.hpp"
#include "system_fsm.hpp"
#include "ui_fsm.hpp"

#include "i2c.hpp"
#include "touchwheel.hpp"

namespace system_fsm {
namespace states {

static const char kTag[] = "BOOT";

static std::function<void(void)> sGpiosCallback = []() {
  events::EventQueue::GetInstance().DispatchFromISR(internal::GpioInterrupt{});
};

auto Booting::entry() -> void {
  ESP_LOGI(kTag, "beginning tangara boot");
  ESP_LOGI(kTag, "installing early drivers");

  // I2C and SPI are both always needed. We can't even power down or show an
  // error without these.
  ESP_ERROR_CHECK(drivers::init_i2c());
  ESP_ERROR_CHECK(drivers::init_spi());

  // These drivers are the bare minimum to even show an error. If these fail,
  // then the system is completely hosed.
  sGpioExpander.reset(drivers::GpioExpander::Create());
  assert(sGpioExpander != nullptr);

  // Start bringing up LVGL now, since we have all of its prerequisites.
  ESP_LOGI(kTag, "starting ui");
  if (!ui::UiState::Init(sGpioExpander.get())) {
    events::Dispatch<FatalError, SystemState, ui::UiState, audio::AudioState>(
        FatalError());
    return;
  }

  // Install everything else that is certain to be needed.
  ESP_LOGI(kTag, "installing remaining drivers");
  sSamd.reset(drivers::Samd::Create());
  sBattery.reset(drivers::Battery::Create());

  if (!sSamd || !sBattery) {
    events::Dispatch<FatalError, SystemState, ui::UiState, audio::AudioState>(
        FatalError());
    return;
  }

  // At this point we've done all of the essential boot tasks. Start remaining
  // state machines and inform them that the system is ready.

  ESP_LOGI(kTag, "starting audio");
  if (!audio::AudioState::Init(sGpioExpander.get(), sDatabase)) {
    events::Dispatch<FatalError, SystemState, ui::UiState, audio::AudioState>(
        FatalError());
    return;
  }

  events::Dispatch<BootComplete, SystemState, ui::UiState, audio::AudioState>(
      BootComplete());
}

auto Booting::exit() -> void {
  // TODO(jacqueline): Gate this on something. Debug flag? Flashing mode?
  sAppConsole = new console::AppConsole();
  sAppConsole->Launch();
}

auto Booting::react(const BootComplete& ev) -> void {
  ESP_LOGI(kTag, "bootup completely successfully");

  // It's possible that the SAMD is currently exposing the SD card as a USB
  // device. Make sure we don't immediately try to claim it.
  if (sSamd && sSamd->ReadUsbMscStatus() ==
                   drivers::Samd::UsbMscStatus::kAttachedMounted) {
    transit<Unmounted>();
  }
  transit<Running>();
}

}  // namespace states
}  // namespace system_fsm
