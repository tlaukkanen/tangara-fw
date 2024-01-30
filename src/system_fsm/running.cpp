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
#include "freertos/projdefs.h"
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

static database::IFileGatherer* sFileGatherer;

void Running::entry() {
  if (mountStorage()) {
    events::Ui().Dispatch(StorageMounted{});
  }
}

void Running::exit() {
  unmountStorage();
}

void Running::react(const KeyLockChanged& ev) {
  if (IdleCondition()) {
    transit<Idle>();
  }
}

void Running::react(const audio::PlaybackFinished& ev) {
  if (IdleCondition()) {
    transit<Idle>();
  }
}

void Running::react(const database::event::UpdateFinished&) {
  if (IdleCondition()) {
    transit<Idle>();
  }
}

void Running::react(const SdDetectChanged& ev) {
  if (ev.has_sd_card) {
    if (!sStorage && mountStorage()) {
      events::Ui().Dispatch(StorageMounted{});
    }
  } else {
    unmountStorage();
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
  return true;
}

auto Running::unmountStorage() -> void {
  sServices->database({});
  sStorage.reset();
}

}  // namespace states
}  // namespace system_fsm
