/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

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

/*
 * Ensure the storage and database are both available. If either of these fails
 * to open, then we assume it's an issue with the underlying SD card.
 */
void Running::entry() {
  ESP_LOGI(kTag, "mounting sd card");
  auto storage_res = drivers::SdStorage::Create(sGpioExpander.get());
  if (storage_res.has_error()) {
    events::Dispatch<StorageError, SystemState, audio::AudioState, ui::UiState>(
        StorageError());
    return;
  }
  sStorage.reset(storage_res.value());

  ESP_LOGI(kTag, "opening database");
  auto database_res = database::Database::Open();
  if (database_res.has_error()) {
    events::Dispatch<StorageError, SystemState, audio::AudioState, ui::UiState>(
        StorageError());
    return;
  }
  sDatabase.reset(database_res.value());

  ESP_LOGI(kTag, "storage loaded okay");
  events::Dispatch<StorageMounted, SystemState, audio::AudioState, ui::UiState>(
      StorageMounted());
}

void Running::exit() {
  sDatabase.reset();
  sStorage.reset();
}

void Running::react(const StorageUnmountRequested& ev) {
  events::Dispatch<internal::ReadyToUnmount, SystemState>(
      internal::ReadyToUnmount());
}

void Running::react(const internal::ReadyToUnmount& ev) {
  transit<Unmounted>();
}

void Running::react(const StorageError& ev) {
  ESP_LOGW(kTag, "error loading storage");
  transit<Unmounted>();
}

}  // namespace states
}  // namespace system_fsm