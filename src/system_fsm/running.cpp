/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "app_console.hpp"
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
  auto storage_res = drivers::SdStorage::Create(sGpios.get());
  if (storage_res.has_error()) {
    ESP_LOGW(kTag, "failed to mount!");

    events::System().Dispatch(StorageError{});
    events::Audio().Dispatch(StorageError{});
    events::Ui().Dispatch(StorageError{});
    return;
  }
  sStorage.reset(storage_res.value());
  vTaskDelay(pdMS_TO_TICKS(250));

  ESP_LOGI(kTag, "opening database");
  sFileGatherer = new database::FileGathererImpl();
  auto database_res = database::Database::Open(sFileGatherer, sTagParser.get());
  if (database_res.has_error()) {
    ESP_LOGW(kTag, "failed to open!");
    events::System().Dispatch(StorageError{});
    events::Audio().Dispatch(StorageError{});
    events::Ui().Dispatch(StorageError{});
    return;
  }
  sDatabase.reset(database_res.value());
  console::AppConsole::sDatabase = sDatabase;

  ESP_LOGI(kTag, "storage loaded okay");
  StorageMounted ev{.db = sDatabase};
  events::System().Dispatch(ev);
  events::Audio().Dispatch(ev);
  events::Ui().Dispatch(ev);
}

void Running::exit() {
  sDatabase.reset();
  sStorage.reset();
}

void Running::react(const KeyLockChanged& ev) {
  if (!ev.falling && audio::AudioState::is_in_state<audio::states::Standby>()) {
    transit<Idle>();
  }
}

void Running::react(const StorageError& ev) {
  ESP_LOGW(kTag, "error loading storage");
  // TODO.
}

}  // namespace states
}  // namespace system_fsm
