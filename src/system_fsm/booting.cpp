#include "assert.h"
#include "audio_fsm.hpp"
#include "core/lv_obj.h"
#include "display_init.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "event_queue.hpp"
#include "gpio_expander.hpp"
#include "lvgl/lvgl.h"
#include "spi.hpp"
#include "system_events.hpp"
#include "system_fsm.hpp"
#include "ui_fsm.hpp"

#include "i2c.hpp"
#include "touchwheel.hpp"

namespace system_fsm {
namespace states {

static const char kTag[] = "BOOT";

console::AppConsole *Booting::sAppConsole;

auto Booting::entry() -> void {
  ESP_LOGI(kTag, "beginning tangara boot");
  ESP_LOGI(kTag, "installing bare minimum drivers");

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
  lv_init();
  sDisplay.reset(drivers::Display::Create(sGpioExpander.get(),
                                          drivers::displays::kST7735R));
  assert(sDisplay != nullptr);

  // The UI FSM now has everything it needs to start setting up. Do this now,
  // so that we can properly show the user any errors that appear later.
  ui::UiState::Init(sGpioExpander.get(), sTouch, sDisplay, sDatabase);
  events::Dispatch<DisplayReady, ui::UiState>(DisplayReady());

  // These drivers are required for normal operation, but aren't critical for
  // booting. We will transition to the error state if these aren't present.
  ESP_LOGI(kTag, "installing required drivers");
  sSamd.reset(drivers::Samd::Create());
  sTouch.reset(drivers::TouchWheel::Create());

  auto dac_res = drivers::AudioDac::create(sGpioExpander.get());
  if (dac_res.has_error() || !sSamd || !sTouch) {
    events::Dispatch<FatalError, SystemState, ui::UiState, audio::AudioState>(
        FatalError());
    return;
  }
  sDac.reset(dac_res.value());

  // These drivers are initialised on boot, but are recoverable (if weird) if
  // they fail.
  ESP_LOGI(kTag, "installing extra drivers");
  sBattery.reset(drivers::Battery::Create());

  // All drivers are now loaded, so we can finish initing the other state
  // machines.
  audio::AudioState::Init(sGpioExpander.get(), sDac, sDatabase);

  events::Dispatch<BootComplete, SystemState, ui::UiState, audio::AudioState>(
      BootComplete());
}

auto Booting::exit() -> void {
  // TODO(jacqueline): Gate this on something. Debug flag? Flashing mode?
  sAppConsole = new console::AppConsole(sDatabase);
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
