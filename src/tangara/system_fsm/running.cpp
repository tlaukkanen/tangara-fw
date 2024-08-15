/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "app_console/app_console.hpp"
#include "audio/audio_events.hpp"
#include "database/database.hpp"
#include "database/db_events.hpp"
#include "drivers/gpios.hpp"
#include "drivers/spi.hpp"
#include "ff.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "result.hpp"

#include "audio/audio_fsm.hpp"
#include "drivers/storage.hpp"
#include "events/event_queue.hpp"
#include "system_fsm/system_events.hpp"
#include "system_fsm/system_fsm.hpp"
#include "ui/ui_fsm.hpp"

namespace system_fsm {
namespace states {

[[maybe_unused]] static const char kTag[] = "RUN";

static const TickType_t kTicksBeforeUnmount = pdMS_TO_TICKS(10000);

static TimerHandle_t sUnmountTimer = nullptr;

static void timer_callback(TimerHandle_t timer) {
  events::System().Dispatch(internal::UnmountTimeout{});
}

void Running::entry() {
  if (!sUnmountTimer) {
    sUnmountTimer = xTimerCreate("unmount_timeout", kTicksBeforeUnmount, false,
                                 NULL, timer_callback);
  }
  mountStorage();
}

void Running::exit() {
  unmountStorage();
}

void Running::react(const KeyLockChanged& ev) {
  checkIdle();
}

void Running::react(const audio::PlaybackUpdate& ev) {
  checkIdle();
}

void Running::react(const database::event::UpdateFinished&) {
  checkIdle();
}

void Running::react(const internal::UnmountTimeout&) {
  if (IdleCondition()) {
    transit<Idle>();
  }
}

void Running::react(const SdDetectChanged& ev) {
  if (sServices->samd().UsbMassStorage()) {
    // We don't currently control the sd card, so don't mess with it.
    return;
  }

  if (ev.has_sd_card && !sStorage) {
    mountStorage();
  }

  // Don't automatically unmount, since this event seems to occasionally happen
  // supriously. FIXME: Why?
  // Instead, check whether or not the card has actually gone away.
  if (sStorage) {
    FF_DIR dir;
    FRESULT res = f_opendir(&dir, "/");
    if (res != FR_OK) {
      ESP_LOGW(kTag, "sd card ejected unsafely!");
      unmountStorage();
    }

    f_closedir(&dir);
  }
}

void Running::react(const SamdUsbMscChanged& ev) {
  if (ev.en) {
    // Stop using the sd card, and power it off.
    unmountStorage();

    // Set up the SD card for usage by the samd21.
    auto& gpios = sServices->gpios();
    gpios.WriteSync(drivers::IGpios::Pin::kSdPowerEnable, 1);
    gpios.WriteSync(drivers::IGpios::Pin::kSdMuxSwitch,
                    drivers::IGpios::SD_MUX_SAMD);
    gpios.WriteSync(drivers::IGpios::Pin::kSdMuxDisable, 0);

    // Off you go!
    sServices->samd().UsbMassStorage(true);
  } else {
    // Make sure the samd knows that its access is going away, and give it time
    // to finish up any remaining work.
    sServices->samd().UsbMassStorage(false);
    vTaskDelay(pdMS_TO_TICKS(250));

    auto& gpios = sServices->gpios();
    // No more writing, please!
    gpios.WriteSync(drivers::IGpios::Pin::kSdMuxDisable, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Reboot the SD card so that it comes up in a consistent state.
    // TODO: can we avoid doing this?
    gpios.WriteSync(drivers::IGpios::Pin::kSdPowerEnable, 0);

    // Now it's ready for us.
    mountStorage();
  }
}

void Running::react(const StorageError& ev) {
  ESP_LOGE(kTag, "storage error %u", ev.error);
  if (ev.error == FR_DISK_ERR || ev.error == FR_INVALID_OBJECT) {
    unmountStorage();
  }
}

auto Running::checkIdle() -> void {
  xTimerStop(sUnmountTimer, portMAX_DELAY);
  if (IdleCondition()) {
    xTimerStart(sUnmountTimer, portMAX_DELAY);
  }
}

auto Running::updateSdState(drivers::SdState state) -> void {
  sServices->sd(state);
  events::Ui().Dispatch(SdStateChanged{});
  events::Audio().Dispatch(SdStateChanged{});
  events::System().Dispatch(SdStateChanged{});
}

auto Running::mountStorage() -> void {
  // Only mount our storage if we know it's not currently in use by the SAMD.
  if (sServices->samd().UsbMassStorage()) {
    updateSdState(drivers::SdState::kNotMounted);
    return;
  }

  ESP_LOGI(kTag, "mounting sd card");
  auto storage_res = drivers::SdStorage::Create(sServices->gpios());
  if (storage_res.has_error()) {
    ESP_LOGW(kTag, "failed to mount!");
    switch (storage_res.error()) {
      case drivers::SdStorage::FAILED_TO_MOUNT:
        updateSdState(drivers::SdState::kNotFormatted);
        break;
      case drivers::SdStorage::FAILED_TO_READ:
      default:
        updateSdState(drivers::SdState::kNotPresent);
        break;
    }
    return;
  }

  sStorage.reset(storage_res.value());

  ESP_LOGI(kTag, "opening database");
  auto database_res = database::Database::Open(
      sServices->tag_parser(), sServices->collator(), sServices->bg_worker());
  if (database_res.has_error()) {
    unmountStorage();
    return;
  }

  sServices->database(
      std::unique_ptr<database::Database>{database_res.value()});

  ESP_LOGI(kTag, "storage loaded okay");
  updateSdState(drivers::SdState::kMounted);

  // Tell the database to refresh so that we pick up any changes from the newly
  // mounted card.
  if (sServices->nvs().DbAutoIndex()) {
    sServices->bg_worker().Dispatch<void>([&]() {
      // Delay the index update for a bit, since we don't want to cause a lot
      // of disk contention immediately after mounting (especially when we've
      // just booted), or else we risk slowing down stuff like UI loading.
      vTaskDelay(pdMS_TO_TICKS(6000));
      auto db = sServices->database().lock();
      if (!db) {
        return;
      }
      db->updateIndexes();
    });
  }
}

auto Running::unmountStorage() -> void {
  ESP_LOGW(kTag, "unmounting storage");
  sServices->database({});
  sStorage.reset();
  updateSdState(drivers::SdState::kNotMounted);
}

}  // namespace states
}  // namespace system_fsm
