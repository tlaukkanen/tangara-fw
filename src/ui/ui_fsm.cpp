/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ui_fsm.hpp"
#include <memory>
#include "audio_events.hpp"
#include "core/lv_obj.h"
#include "display.hpp"
#include "event_queue.hpp"
#include "lvgl_task.hpp"
#include "relative_wheel.hpp"
#include "screen.hpp"
#include "screen_menu.hpp"
#include "screen_playing.hpp"
#include "screen_splash.hpp"
#include "screen_track_browser.hpp"
#include "system_events.hpp"
#include "touchwheel.hpp"
#include "track_queue.hpp"

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
}

void UiState::PopScreen() {
  if (sScreens.empty()) {
    return;
  }
  sCurrentScreen = sScreens.top();
  sScreens.pop();
}

namespace states {

void Splash::exit() {
  if (sDisplay != nullptr) {
    sDisplay->SetDisplayOn(true);
  }
}

void Splash::react(const system_fsm::BootComplete& ev) {
  transit<Browse>();
}

void Browse::entry() {}

void Browse::react(const system_fsm::KeyLockChanged& ev) {
  sDisplay->SetDisplayOn(ev.falling);
}

void Browse::react(const system_fsm::StorageMounted& ev) {
  sDb = ev.db;
  auto db = ev.db.lock();
  if (!db) {
    // TODO(jacqueline): Hmm.
    return;
  }
  PushScreen(std::make_shared<screens::Menu>(db->GetIndexes()));
}

void Browse::react(const internal::RecordSelected& ev) {
  auto db = sDb.lock();
  if (!db) {
    return;
  }

  if (ev.record.track()) {
    ESP_LOGI(kTag, "selected track '%s'", ev.record.text()->c_str());
    // TODO(jacqueline): We should also send some kind of playlist info here.
    sQueue->Clear();
    sQueue->AddLast(*ev.record.track());
    transit<Playing>();
  } else {
    ESP_LOGI(kTag, "selected record '%s'", ev.record.text()->c_str());
    auto cont = ev.record.Expand(kRecordsPerPage);
    if (!cont) {
      return;
    }
    auto query = db->GetPage(&cont.value());
    std::string title = ev.record.text().value_or("TODO");
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

}  // namespace states
}  // namespace ui

FSM_INITIAL_STATE(ui::UiState, ui::states::Splash)
