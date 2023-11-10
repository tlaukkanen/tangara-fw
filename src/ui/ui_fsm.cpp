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
#include "database.hpp"
#include "haptics.hpp"
#include "misc/lv_gc.h"

#include "audio_events.hpp"
#include "display.hpp"
#include "encoder_input.hpp"
#include "event_queue.hpp"
#include "gpios.hpp"
#include "lvgl_task.hpp"
#include "modal_add_to_queue.hpp"
#include "modal_confirm.hpp"
#include "modal_progress.hpp"
#include "model_playback.hpp"
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
#include "storage.hpp"
#include "system_events.hpp"
#include "touchwheel.hpp"
#include "track_queue.hpp"
#include "ui_events.hpp"
#include "widget_top_bar.hpp"

namespace ui {

[[maybe_unused]] static constexpr char kTag[] = "ui_fsm";

static const std::size_t kRecordsPerPage = 15;

std::unique_ptr<UiTask> UiState::sTask;
std::shared_ptr<system_fsm::ServiceLocator> UiState::sServices;
std::unique_ptr<drivers::Display> UiState::sDisplay;
std::shared_ptr<EncoderInput> UiState::sInput;

std::stack<std::shared_ptr<Screen>> UiState::sScreens;
std::shared_ptr<Screen> UiState::sCurrentScreen;
std::shared_ptr<Modal> UiState::sCurrentModal;

models::Playback UiState::sPlaybackModel;
models::TopBar UiState::sTopBarModel{{},
                                     UiState::sPlaybackModel.is_playing,
                                     UiState::sPlaybackModel.current_track};

auto UiState::InitBootSplash(drivers::IGpios& gpios) -> bool {
  // Init LVGL first, since the display driver registers itself with LVGL.
  lv_init();
  sDisplay.reset(drivers::Display::Create(gpios, drivers::displays::kST7735R));
  if (sDisplay == nullptr) {
    return false;
  }

  sCurrentScreen.reset(new screens::Splash());
  sTask.reset(UiTask::Start());
  sDisplay->SetDisplayOn(!gpios.Get(drivers::IGpios::Pin::kKeyLock));
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

void UiState::react(const system_fsm::KeyLockChanged& ev) {
  sDisplay->SetDisplayOn(!ev.locking);
  sInput->lock(ev.locking);
}

void UiState::react(const system_fsm::BatteryStateChanged& ev) {
  sTopBarModel.battery_state.set(ev.new_state);
}

void UiState::react(const audio::PlaybackStarted&) {
  sPlaybackModel.is_playing.set(true);
}

void UiState::react(const audio::PlaybackFinished&) {
  sPlaybackModel.is_playing.set(false);
}

void UiState::react(const audio::PlaybackUpdate& ev) {
  sPlaybackModel.current_track_duration.set(ev.seconds_total);
  sPlaybackModel.current_track_position.set(ev.seconds_elapsed);
}

void UiState::react(const audio::QueueUpdate&) {
  auto& queue = sServices->track_queue();
  bool had_queue = sPlaybackModel.current_track.get().has_value();
  sPlaybackModel.current_track.set(queue.GetCurrent());
  sPlaybackModel.upcoming_tracks.set(queue.GetUpcoming(10));
  if (!had_queue) {
    transit<states::Playing>();
  }
}

void UiState::react(const internal::ControlSchemeChanged&) {
  if (!sInput) {
    return;
  }
  sInput->mode(sServices->nvs().PrimaryInput());
}

namespace states {

void Splash::exit() {
  // buzz a bit to tell the user we're done booting
  events::System().Dispatch(system_fsm::HapticTrigger{
      .effect = drivers::Haptics::Effect::kLongDoubleSharpTick1_100Pct,
  });
}

void Splash::react(const system_fsm::BootComplete& ev) {
  sServices = ev.services;

  // The system has finished booting! We now need to prepare to show real UI.
  // This basically just involves reading and applying the user's preferences.

  lv_theme_t* base_theme = lv_theme_basic_init(NULL);
  lv_disp_set_theme(NULL, base_theme);
  themes::Theme::instance()->Apply();

  sDisplay->SetBrightness(sServices->nvs().ScreenBrightness());

  auto touchwheel = sServices->touchwheel();
  if (touchwheel) {
    sInput = std::make_shared<EncoderInput>(sServices->gpios(), **touchwheel);
    sInput->mode(sServices->nvs().PrimaryInput());
    sTask->input(sInput);
  } else {
    ESP_LOGE(kTag, "no input devices initialised!");
  }

  if (sServices->nvs().HasShownOnboarding()) {
    transit<Browse>();
  } else {
    transit<Onboarding>();
  }
}

void Onboarding::entry() {
  progress_ = 0;
  has_formatted_ = false;
  sCurrentScreen.reset(new screens::onboarding::LinkToManual());
}

void Onboarding::react(const internal::OnboardingNavigate& ev) {
  int dir = ev.forwards ? 1 : -1;
  progress_ += dir;

  for (;;) {
    if (progress_ == 0) {
      sCurrentScreen.reset(new screens::onboarding::LinkToManual());
      return;
    } else if (progress_ == 1) {
      sCurrentScreen.reset(new screens::onboarding::Controls());
      return;
    } else if (progress_ == 2) {
      if (sServices->sd() == drivers::SdState::kNotPresent) {
        sCurrentScreen.reset(new screens::onboarding::MissingSdCard());
        return;
      } else {
        progress_ += dir;
      }
    } else if (progress_ == 3) {
      if (sServices->sd() == drivers::SdState::kNotFormatted) {
        has_formatted_ = true;
        sCurrentScreen.reset(new screens::onboarding::FormatSdCard());
        return;
      } else {
        progress_ += dir;
      }
    } else if (progress_ == 4) {
      if (has_formatted_) {
        // If we formatted the SD card during this onboarding flow, then there
        // is no music that needs indexing.
        progress_ += dir;
      } else {
        sCurrentScreen.reset(new screens::onboarding::InitDatabase());
        return;
      }
    } else {
      // We finished onboarding! Ensure this flow doesn't appear again.
      sServices->nvs().HasShownOnboarding(true);

      transit<Browse>();
      return;
    }
  }
}

static bool sBrowseFirstEntry = true;

void Browse::entry() {
  if (sBrowseFirstEntry) {
    auto db = sServices->database().lock();
    if (!db) {
      return;
    }
    sCurrentScreen =
        std::make_shared<screens::Menu>(sTopBarModel, db->GetIndexes());
    sBrowseFirstEntry = false;
  }
}

void Browse::react(const system_fsm::StorageMounted& ev) {
  if (sBrowseFirstEntry) {
    auto db = sServices->database().lock();
    if (!db) {
      return;
    }
    sCurrentScreen =
        std::make_shared<screens::Menu>(sTopBarModel, db->GetIndexes());
    sBrowseFirstEntry = false;
  }
}

void Browse::react(const internal::ShowNowPlaying& ev) {
  transit<Playing>();
}

void Browse::react(const internal::ShowSettingsPage& ev) {
  std::shared_ptr<Screen> screen;
  std::shared_ptr<screens::Bluetooth> bt_screen;
  switch (ev.page) {
    case internal::ShowSettingsPage::Page::kRoot:
      screen.reset(new screens::Settings(sTopBarModel));
      break;
    case internal::ShowSettingsPage::Page::kBluetooth:
      bt_screen = std::make_shared<screens::Bluetooth>(
          sTopBarModel, sServices->bluetooth(), sServices->nvs());
      screen = bt_screen;
      bluetooth_screen_ = bt_screen;
      break;
    case internal::ShowSettingsPage::Page::kHeadphones:
      screen.reset(new screens::Headphones(sTopBarModel, sServices->nvs()));
      break;
    case internal::ShowSettingsPage::Page::kAppearance:
      screen.reset(
          new screens::Appearance(sTopBarModel, sServices->nvs(), *sDisplay));
      break;
    case internal::ShowSettingsPage::Page::kInput:
      screen.reset(new screens::InputMethod(sTopBarModel, sServices->nvs()));
      break;
    case internal::ShowSettingsPage::Page::kStorage:
      screen.reset(new screens::Storage(sTopBarModel));
      break;
    case internal::ShowSettingsPage::Page::kFirmwareUpdate:
      screen.reset(new screens::FirmwareUpdate(sTopBarModel));
      break;
    case internal::ShowSettingsPage::Page::kAbout:
      screen.reset(new screens::About(sTopBarModel));
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

  auto& queue = sServices->track_queue();
  auto record = ev.page->values().at(ev.record);
  if (record->track()) {
    ESP_LOGI(kTag, "selected track '%s'", record->text()->c_str());
    auto source = std::make_shared<playlist::IndexRecordSource>(
        sServices->database(), ev.initial_page, 0, ev.page, ev.record);
    if (ev.show_menu) {
      sCurrentModal.reset(
          new modals::AddToQueue(sCurrentScreen.get(), queue, source));
    } else {
      queue.Clear();
      queue.AddNext(source);
      transit<Playing>();
    }
  } else {
    ESP_LOGI(kTag, "selected record '%s'", record->text()->c_str());
    auto cont = record->Expand(kRecordsPerPage);
    if (!cont) {
      return;
    }
    auto query = db->GetPage<database::IndexRecord>(&cont.value());
    if (ev.show_menu) {
      std::shared_ptr<database::Result<database::IndexRecord>> res{query.get()};
      auto source = playlist::CreateSourceFromResults(db, res);
      sCurrentModal.reset(
          new modals::AddToQueue(sCurrentScreen.get(), queue, source, true));
    } else {
      std::pmr::string title = record->text().value_or("");
      PushScreen(std::make_shared<screens::TrackBrowser>(
          sTopBarModel, sServices->track_queue(), sServices->database(),
          ev.new_crumbs, std::move(query)));
    }
  }
}

void Browse::react(const internal::IndexSelected& ev) {
  auto db = sServices->database().lock();
  if (!db) {
    return;
  }

  ESP_LOGI(kTag, "selected index %s", ev.index.name.c_str());
  auto query = db->GetTracksByIndex(ev.index, kRecordsPerPage);
  std::pmr::vector<std::pmr::string> crumbs = {ev.index.name};
  PushScreen(std::make_shared<screens::TrackBrowser>(
      sTopBarModel, sServices->track_queue(), sServices->database(), crumbs,
      std::move(query)));
}

void Browse::react(const internal::BackPressed& ev) {
  PopScreen();
}

void Browse::react(const system_fsm::BluetoothDevicesChanged&) {
  auto bt = bluetooth_screen_.lock();
  if (bt) {
    bt->RefreshDevicesList();
  }
}

void Browse::react(const internal::ReindexDatabase& ev) {
  transit<Indexing>();
}

static std::shared_ptr<screens::Playing> sPlayingScreen;

void Playing::entry() {
  ESP_LOGI(kTag, "push playing screen");
  sPlayingScreen.reset(new screens::Playing(sTopBarModel, sPlaybackModel,
                                            sServices->database(),
                                            sServices->track_queue()));
  PushScreen(sPlayingScreen);
}

void Playing::exit() {
  sPlayingScreen.reset();
  PopScreen();
}

void Playing::react(const internal::BackPressed& ev) {
  transit<Browse>();
}

static std::shared_ptr<modals::Progress> sIndexProgress;

void Indexing::entry() {
  sIndexProgress.reset(new modals::Progress(sCurrentScreen.get(), "Indexing",
                                            "Preparing database"));
  sCurrentModal = sIndexProgress;
  auto db = sServices->database().lock();
  if (!db) {
    // TODO: Hmm.
    return;
  }
  db->Update();
}

void Indexing::exit() {
  sCurrentModal.reset();
  sIndexProgress.reset();
}

void Indexing::react(const database::event::UpdateStarted&) {}

void Indexing::react(const database::event::UpdateProgress& ev) {
  std::ostringstream str;
  switch (ev.stage) {
    case database::event::UpdateProgress::Stage::kVerifyingExistingTracks:
      sIndexProgress->title("Verifying");
      str << "Tracks checked: " << ev.val;
      sIndexProgress->subtitle(str.str().c_str());
      break;
    case database::event::UpdateProgress::Stage::kScanningForNewTracks:
      sIndexProgress->title("Scanning");
      str << "Files checked: " << ev.val;
      sIndexProgress->subtitle(str.str().c_str());
      break;
  }
}

void Indexing::react(const database::event::UpdateFinished&) {
  transit<Browse>();
}

}  // namespace states
}  // namespace ui

FSM_INITIAL_STATE(ui::UiState, ui::states::Splash)
