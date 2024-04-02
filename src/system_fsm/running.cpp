/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "app_console.hpp"
#include "audio_events.hpp"
#include "database.hpp"
#include "db_events.hpp"
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

[[maybe_unused]] static const char kTag[] = "RUN";

static const TickType_t kTicksBeforeUnmount = pdMS_TO_TICKS(10000);

static TimerHandle_t sUnmountTimer = nullptr;

static void timer_callback(TimerHandle_t timer) {
  events::System().Dispatch(internal::UnmountTimeout{});
}

static database::IFileGatherer* sFileGatherer;

void Running::entry() {
  if (!sUnmountTimer) {
    sUnmountTimer = xTimerCreate("unmount_timeout", kTicksBeforeUnmount, false,
                                 NULL, timer_callback);
  }
  // Only mount our storage immediately if we know it's not currently in use
  // by the SAMD.
  if (!sServices->samd().UsbMassStorage()) {
    mountStorage();
  }
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
  // (It doesn't matter too much; by the time we get this event the SD card has
  // already been disconnected electrically.)
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

auto Running::checkIdle() -> void {
  xTimerStop(sUnmountTimer, portMAX_DELAY);
  if (IdleCondition()) {
    xTimerStart(sUnmountTimer, portMAX_DELAY);
  }
}

auto Running::mountStorage() -> bool {
  ESP_LOGI(kTag, "mounting sd card");
  auto storage_res = drivers::SdStorage::Create(sServices->gpios());
  if (storage_res.has_error()) {
    ESP_LOGW(kTag, "failed to mount!");
    switch (storage_res.error()) {
      case drivers::SdStorage::FAILED_TO_MOUNT:
        sServices->sd(drivers::SdState::kNotFormatted);
        break;
      case drivers::SdStorage::FAILED_TO_READ:
      default:
        sServices->sd(drivers::SdState::kNotPresent);
        break;
    }
    return false;
  }

  sStorage.reset(storage_res.value());
  sServices->sd(drivers::SdState::kMounted);

  ESP_LOGI(kTag, "opening database");
  sFileGatherer = new database::FileGathererImpl();
  auto database_res =
      database::Database::Open(*sFileGatherer, sServices->tag_parser(),
                               sServices->collator(), sServices->bg_worker());
  if (database_res.has_error()) {
    unmountStorage();
    return false;
  }

  sServices->database(
      std::unique_ptr<database::Database>{database_res.value()});

  ESP_LOGI(kTag, "storage loaded okay");
  events::Ui().Dispatch(StorageMounted{});
  events::Audio().Dispatch(StorageMounted{});
  events::System().Dispatch(StorageMounted{});

  // Tell the database to refresh so that we pick up any changes from the newly
  // mounted card.
  if (sServices->nvs().DbAutoIndex()) {
    sServices->bg_worker().Dispatch<void>([&]() {
      auto db = sServices->database().lock();
      if (!db) {
        return;
      }
      db->updateIndexes();
    });
  }

  return true;
}

auto Running::unmountStorage() -> void {
  ESP_LOGW(kTag, "unmounting storage");
  sServices->database({});
  sStorage.reset();
}

}  // namespace states
}  // namespace system_fsm
