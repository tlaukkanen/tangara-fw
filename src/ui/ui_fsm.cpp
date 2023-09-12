/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ui_fsm.hpp"

#include <memory>

#include "audio_fsm.hpp"
#include "battery.hpp"
#include "core/lv_obj.h"
#include "misc/lv_gc.h"

#include "audio_events.hpp"
#include "display.hpp"
#include "event_queue.hpp"
#include "gpios.hpp"
#include "lvgl_task.hpp"
#include "modal_confirm.hpp"
#include "nvs.hpp"
#include "relative_wheel.hpp"
#include "screen.hpp"
#include "screen_menu.hpp"
#include "screen_onboarding.hpp"
#include "screen_playing.hpp"
#include "screen_settings.hpp"
#include "screen_splash.hpp"
#include "screen_track_browser.hpp"
#include "source.hpp"
#include "system_events.hpp"
#include "touchwheel.hpp"
#include "track_queue.hpp"
#include "ui_events.hpp"
#include "wheel_encoder.hpp"
#include "widget_top_bar.hpp"

namespace ui {

static constexpr char kTag[] = "ui_fsm";

static const std::size_t kRecordsPerPage = 15;

std::unique_ptr<UiTask> UiState::sTask;
std::shared_ptr<system_fsm::ServiceLocator> UiState::sServices;
std::unique_ptr<drivers::Display> UiState::sDisplay;
std::shared_ptr<TouchWheelEncoder> UiState::sEncoder;

std::stack<std::shared_ptr<Screen>> UiState::sScreens;
std::shared_ptr<Screen> UiState::sCurrentScreen;
std::shared_ptr<Modal> UiState::sCurrentModal;

auto UiState::InitBootSplash(drivers::IGpios& gpios) -> bool {
  // Init LVGL first, since the display driver registers itself with LVGL.
  lv_init();
  sDisplay.reset(drivers::Display::Create(gpios, drivers::displays::kST7735R));
  if (sDisplay == nullptr) {
    return false;
  }

  sCurrentScreen.reset(new screens::Splash());
  sTask.reset(UiTask::Start());
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
  sTask->SetInputDevice(ev.falling ? sEncoder
                                   : std::shared_ptr<TouchWheelEncoder>());
}

void UiState::react(const system_fsm::BatteryStateChanged&) {
  UpdateTopBar();
}

void UiState::UpdateTopBar() {
  auto battery_state = sServices->battery().State();
  bool has_queue = sServices->track_queue().GetCurrent().has_value();
  bool is_playing = audio::AudioState::is_in_state<audio::states::Playback>();

  widgets::TopBar::State state{
      .playback_state = widgets::TopBar::PlaybackState::kIdle,
      .battery_percent = static_cast<uint_fast8_t>(
          battery_state.has_value() ? battery_state->percent : 100),
      .is_charging = !battery_state.has_value() || battery_state->is_charging,
  };

  if (has_queue) {
    state.playback_state = is_playing ? widgets::TopBar::PlaybackState::kPlaying
                                      : widgets::TopBar::PlaybackState::kPaused;
  }

  if (sCurrentScreen) {
    sCurrentScreen->UpdateTopBar(state);
  }
}

namespace states {

void Splash::exit() {
  if (sDisplay != nullptr) {
    sDisplay->SetDisplayOn(
        sServices->gpios().Get(drivers::IGpios::Pin::kKeyLock));
  }
}

void Splash::react(const system_fsm::BootComplete& ev) {
  sServices = ev.services;

  // The system has finished booting! We now need to prepare to show real UI.
  // This basically just involves reading and applying the user's preferences.

  lv_theme_t* base_theme = lv_theme_basic_init(NULL);
  lv_disp_set_theme(NULL, base_theme);
  themes::Theme::instance()->Apply();

  sDisplay->SetBrightness(sServices->nvs().ScreenBrightness().get());

  auto touchwheel = sServices->touchwheel();
  if (touchwheel) {
    auto relative_wheel =
        std::make_unique<drivers::RelativeWheel>(**touchwheel);
    sEncoder = std::make_shared<TouchWheelEncoder>(std::move(relative_wheel));
    sTask->SetInputDevice(sEncoder);
  } else {
    ESP_LOGE(kTag, "no input devices initialised!");
  }

  if (sServices->nvs().HasShownOnboarding().get()) {
    transit<Browse>();
  } else {
    transit<Onboarding>();
  }
}

void Onboarding::entry() {
  sCurrentScreen.reset(new screens::onboarding::LinkToManual());
}

void Browse::entry() {}

void Browse::react(const system_fsm::StorageMounted& ev) {
  auto db = sServices->database().lock();
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
  std::shared_ptr<Screen> screen;
  switch (ev.page) {
    case internal::ShowSettingsPage::Page::kRoot:
      screen.reset(new screens::Settings());
      break;
    case internal::ShowSettingsPage::Page::kBluetooth:
      screen.reset(new screens::Bluetooth());
      break;
    case internal::ShowSettingsPage::Page::kHeadphones:
      screen.reset(new screens::Headphones(sServices->nvs()));
      break;
    case internal::ShowSettingsPage::Page::kAppearance:
      screen.reset(new screens::Appearance(sServices->nvs(), *sDisplay));
      break;
    case internal::ShowSettingsPage::Page::kInput:
      screen.reset(new screens::InputMethod());
      break;
    case internal::ShowSettingsPage::Page::kStorage:
      screen.reset(new screens::Storage());
      break;
    case internal::ShowSettingsPage::Page::kFirmwareUpdate:
      screen.reset(new screens::FirmwareUpdate());
      break;
    case internal::ShowSettingsPage::Page::kAbout:
      screen.reset(new screens::About());
      break;
  }
  if (screen) {
    PushScreen(screen);
  }
}

void Browse::react(const internal::RecordSelected& ev) {
  auto db = sServices->database().lock();
  if (!db) {
    return;
  }

  auto record = ev.page->values().at(ev.record);
  if (record.track()) {
    ESP_LOGI(kTag, "selected track '%s'", record.text()->c_str());
    auto& queue = sServices->track_queue();
    queue.Clear();
    queue.IncludeLast(std::make_shared<playlist::IndexRecordSource>(
        sServices->database(), ev.initial_page, 0, ev.page, ev.record));
    transit<Playing>();
  } else {
    ESP_LOGI(kTag, "selected record '%s'", record.text()->c_str());
    auto cont = record.Expand(kRecordsPerPage);
    if (!cont) {
      return;
    }
    auto query = db->GetPage(&cont.value());
    std::string title = record.text().value_or("TODO");
    PushScreen(std::make_shared<screens::TrackBrowser>(
        sServices->database(), title, std::move(query)));
  }
}

void Browse::react(const internal::IndexSelected& ev) {
  auto db = sServices->database().lock();
  if (!db) {
    return;
  }

  ESP_LOGI(kTag, "selected index %s", ev.index.name.c_str());
  auto query = db->GetTracksByIndex(ev.index, kRecordsPerPage);
  PushScreen(std::make_shared<screens::TrackBrowser>(
      sServices->database(), ev.index.name, std::move(query)));
}

void Browse::react(const internal::BackPressed& ev) {
  PopScreen();
}

static std::shared_ptr<screens::Playing> sPlayingScreen;

void Playing::entry() {
  sPlayingScreen.reset(
      new screens::Playing(sServices->database(), sServices->track_queue()));
  PushScreen(sPlayingScreen);
}

void Playing::exit() {
  sPlayingScreen.reset();
  PopScreen();
}

void Playing::react(const audio::PlaybackStarted& ev) {
  vTaskPrioritySet(NULL, 0);
  UpdateTopBar();
  sPlayingScreen->OnTrackUpdate();
}

void Playing::react(const audio::PlaybackUpdate& ev) {
  sPlayingScreen->OnPlaybackUpdate(ev.seconds_elapsed, ev.seconds_total);
}

void Playing::react(const audio::PlaybackFinished& ev) {
  UpdateTopBar();
  vTaskPrioritySet(NULL, 10);
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
