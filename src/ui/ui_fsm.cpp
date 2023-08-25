/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ui_fsm.hpp"

#include <memory>

#include "core/lv_obj.h"
#include "misc/lv_gc.h"

#include "audio_events.hpp"
#include "display.hpp"
#include "event_queue.hpp"
#include "gpios.hpp"
#include "lvgl_task.hpp"
#include "modal_confirm.hpp"
#include "relative_wheel.hpp"
#include "screen.hpp"
#include "screen_menu.hpp"
#include "screen_playing.hpp"
#include "screen_settings.hpp"
#include "screen_splash.hpp"
#include "screen_track_browser.hpp"
#include "source.hpp"
#include "system_events.hpp"
#include "touchwheel.hpp"
#include "track_queue.hpp"
#include "ui_events.hpp"
#include "widget_top_bar.hpp"

namespace ui {

static constexpr char kTag[] = "ui_fsm";

static const std::size_t kRecordsPerPage = 15;

drivers::IGpios* UiState::sIGpios;
audio::TrackQueue* UiState::sQueue;

std::shared_ptr<drivers::TouchWheel> UiState::sTouchWheel;
std::shared_ptr<drivers::RelativeWheel> UiState::sRelativeWheel;
std::shared_ptr<drivers::Display> UiState::sDisplay;
std::weak_ptr<database::Database> UiState::sDb;

std::stack<std::shared_ptr<Screen>> UiState::sScreens;
std::shared_ptr<Screen> UiState::sCurrentScreen;
std::shared_ptr<Modal> UiState::sCurrentModal;

auto UiState::Init(drivers::IGpios* gpio_expander, audio::TrackQueue* queue)
    -> bool {
  sIGpios = gpio_expander;
  sQueue = queue;

  lv_init();
  sDisplay.reset(
      drivers::Display::Create(gpio_expander, drivers::displays::kST7735R));
  if (sDisplay == nullptr) {
    return false;
  }

  sTouchWheel.reset(drivers::TouchWheel::Create());
  if (sTouchWheel != nullptr) {
    sRelativeWheel.reset(new drivers::RelativeWheel(sTouchWheel.get()));
  }

  sCurrentScreen.reset(new screens::Splash());

  // Start the UI task even if init ultimately failed, so that we can show some
  // kind of error screen to the user.
  StartLvgl(sRelativeWheel, sDisplay);

  if (sTouchWheel == nullptr) {
    return false;
  }
  return true;
}

void UiState::PushScreen(std::shared_ptr<Screen> screen) {
  if (sCurrentScreen) {
    sScreens.push(sCurrentScreen);
  }
  sCurrentScreen = screen;
  UpdateTopBar();
}

void UiState::PopScreen() {
  if (sScreens.empty()) {
    return;
  }
  sCurrentScreen = sScreens.top();
  sScreens.pop();
  UpdateTopBar();
}

void UiState::react(const system_fsm::KeyLockChanged& ev) {
  sDisplay->SetDisplayOn(ev.falling);
}

void UiState::react(const system_fsm::BatteryPercentChanged&) {
  UpdateTopBar();
}

void UiState::UpdateTopBar() {
  widgets::TopBar::State state{
      .playback_state = widgets::TopBar::PlaybackState::kIdle,
      .battery_percent = 50,
      .is_charging = true,
  };
  if (sCurrentScreen) {
    sCurrentScreen->UpdateTopBar(state);
  }
}

namespace states {

void Splash::exit() {
  if (sDisplay != nullptr) {
    sDisplay->SetDisplayOn(sIGpios->Get(drivers::IGpios::Pin::kKeyLock));
  }
}

void Splash::react(const system_fsm::BootComplete& ev) {
  transit<Browse>();
}

void Browse::entry() {}

void Browse::react(const system_fsm::StorageMounted& ev) {
  sDb = ev.db;
  auto db = ev.db.lock();
  if (!db) {
    // TODO(jacqueline): Hmm.
    return;
  }
  PushScreen(std::make_shared<screens::Menu>(db->GetIndexes()));
}

void Browse::react(const internal::ShowNowPlaying& ev) {
  transit<Playing>();
}

void Browse::react(const internal::ShowSettingsPage& ev) {
  PushScreen(ev.screen);
}

void Browse::react(const internal::RecordSelected& ev) {
  auto db = sDb.lock();
  if (!db) {
    return;
  }

  auto record = ev.page->values().at(ev.record);
  if (record.track()) {
    ESP_LOGI(kTag, "selected track '%s'", record.text()->c_str());
    sQueue->Clear();
    sQueue->IncludeLast(std::make_shared<playlist::IndexRecordSource>(
        sDb, ev.initial_page, 0, ev.page, ev.record));
    transit<Playing>();
  } else {
    ESP_LOGI(kTag, "selected record '%s'", record.text()->c_str());
    auto cont = record.Expand(kRecordsPerPage);
    if (!cont) {
      return;
    }
    auto query = db->GetPage(&cont.value());
    std::string title = record.text().value_or("TODO");
    PushScreen(
        std::make_shared<screens::TrackBrowser>(sDb, title, std::move(query)));
  }
}

void Browse::react(const internal::IndexSelected& ev) {
  auto db = sDb.lock();
  if (!db) {
    return;
  }

  ESP_LOGI(kTag, "selected index %s", ev.index.name.c_str());
  auto query = db->GetTracksByIndex(ev.index, kRecordsPerPage);
  PushScreen(std::make_shared<screens::TrackBrowser>(sDb, ev.index.name,
                                                     std::move(query)));
}

void Browse::react(const internal::BackPressed& ev) {
  PopScreen();
}

static std::shared_ptr<screens::Playing> sPlayingScreen;

void Playing::entry() {
  sPlayingScreen.reset(new screens::Playing(sDb, sQueue));
  PushScreen(sPlayingScreen);
}

void Playing::exit() {
  sPlayingScreen.reset();
  PopScreen();
}

void Playing::react(const audio::PlaybackStarted& ev) {
  sPlayingScreen->OnTrackUpdate();
}

void Playing::react(const audio::PlaybackUpdate& ev) {
  sPlayingScreen->OnPlaybackUpdate(ev.seconds_elapsed, ev.seconds_total);
}

void Playing::react(const audio::QueueUpdate& ev) {
  sPlayingScreen->OnQueueUpdate();
}

void Playing::react(const internal::BackPressed& ev) {
  transit<Browse>();
}

}  // namespace states
}  // namespace ui

FSM_INITIAL_STATE(ui::UiState, ui::states::Splash)
