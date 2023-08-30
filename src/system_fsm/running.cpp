/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "app_console.hpp"
#include "audio_events.hpp"
#include "database.hpp"
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

static const char kTag[] = "RUN";

static database::IFileGatherer* sFileGatherer;

/*
 * Ensure the storage and database are both available. If either of these fails
 * to open, then we assume it's an issue with the underlying SD card.
 */
void Running::entry() {
  ESP_LOGI(kTag, "mounting sd card");
  vTaskDelay(pdMS_TO_TICKS(250));
  auto storage_res = drivers::SdStorage::Create(sServices->gpios());
  if (storage_res.has_error()) {
    ESP_LOGW(kTag, "failed to mount!");

    events::System().Dispatch(StorageError{});
    events::Audio().Dispatch(StorageError{});
    events::Ui().Dispatch(StorageError{});
    return;
  }
  sStorage.reset(storage_res.value());

  ESP_LOGI(kTag, "opening database");
  sFileGatherer = new database::FileGathererImpl();
  auto database_res =
      database::Database::Open(*sFileGatherer, sServices->tag_parser());
  if (database_res.has_error()) {
    ESP_LOGW(kTag, "failed to open!");
    events::System().Dispatch(StorageError{});
    events::Audio().Dispatch(StorageError{});
    events::Ui().Dispatch(StorageError{});
    return;
  }
  sServices->database(
      std::unique_ptr<database::Database>{database_res.value()});

  ESP_LOGI(kTag, "storage loaded okay");
  StorageMounted ev{};
  events::System().Dispatch(ev);
  events::Audio().Dispatch(ev);
  events::Ui().Dispatch(ev);
}

void Running::exit() {
  sServices->database({});
  sStorage.reset();
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

void Running::react(const StorageError& ev) {
  ESP_LOGW(kTag, "error loading storage");
  // TODO.
}

}  // namespace states
}  // namespace system_fsm
