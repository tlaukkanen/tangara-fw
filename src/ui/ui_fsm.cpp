/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ui_fsm.hpp"
#include <stdint.h>

#include <memory>
#include <variant>

#include "core/lv_obj_pos.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "lua.h"
#include "lua.hpp"

#include "audio_fsm.hpp"
#include "battery.hpp"
#include "core/lv_group.h"
#include "core/lv_obj.h"
#include "core/lv_obj_tree.h"
#include "database.hpp"
#include "esp_heap_caps.h"
#include "haptics.hpp"
#include "lauxlib.h"
#include "lua_thread.hpp"
#include "luavgl.h"
#include "misc/lv_gc.h"

#include "audio_events.hpp"
#include "display.hpp"
#include "encoder_input.hpp"
#include "event_queue.hpp"
#include "gpios.hpp"
#include "lvgl_task.hpp"
#include "modal_confirm.hpp"
#include "modal_progress.hpp"
#include "model_playback.hpp"
#include "nvs.hpp"
#include "property.hpp"
#include "relative_wheel.hpp"
#include "screen.hpp"
#include "screen_lua.hpp"
#include "screen_settings.hpp"
#include "screen_splash.hpp"
#include "spiffs.hpp"
#include "storage.hpp"
#include "system_events.hpp"
#include "tinyfsm.hpp"
#include "touchwheel.hpp"
#include "track_queue.hpp"
#include "ui_events.hpp"
#include "widgets/lv_arc.h"
#include "widgets/lv_label.h"

namespace ui {

[[maybe_unused]] static constexpr char kTag[] = "ui_fsm";

std::unique_ptr<UiTask> UiState::sTask;
std::shared_ptr<system_fsm::ServiceLocator> UiState::sServices;
std::unique_ptr<drivers::Display> UiState::sDisplay;
std::shared_ptr<EncoderInput> UiState::sInput;

std::stack<std::shared_ptr<Screen>> UiState::sScreens;
std::shared_ptr<Screen> UiState::sCurrentScreen;
std::shared_ptr<Modal> UiState::sCurrentModal;
std::shared_ptr<lua::LuaThread> UiState::sLua;

std::weak_ptr<screens::Bluetooth> UiState::bluetooth_screen_;

models::Playback UiState::sPlaybackModel;
models::TopBar UiState::sTopBarModel{{},
                                     UiState::sPlaybackModel.is_playing,
                                     UiState::sPlaybackModel.current_track};

static TimerHandle_t sAlertTimer;
static lv_obj_t* sAlertContainer;

static void alert_timer_callback(TimerHandle_t timer) {
  events::Ui().Dispatch(internal::DismissAlerts{});
}

static TimerHandle_t sAngleTimer;

static void angle_timer_callback(TimerHandle_t timer) {
  events::Ui().Dispatch(internal::Angle{});
}

auto UiState::InitBootSplash(drivers::IGpios& gpios) -> bool {
  // Init LVGL first, since the display driver registers itself with LVGL.
  lv_init();
  sDisplay.reset(drivers::Display::Create(gpios, drivers::displays::kST7735R));
  if (sDisplay == nullptr) {
    return false;
  }

  sCurrentScreen.reset(new screens::Splash());
  sTask.reset(UiTask::Start());
  sDisplay->SetDisplayOn(!gpios.IsLocked());
  return true;
}

void UiState::PushScreen(std::shared_ptr<Screen> screen) {
  if (sCurrentScreen) {
    sScreens.push(sCurrentScreen);
  }
  sCurrentScreen = screen;
  lv_obj_set_parent(sAlertContainer, sCurrentScreen->alert());
}

int UiState::PopScreen() {
  if (sScreens.empty()) {
    return 0;
  }
  sCurrentScreen = sScreens.top();
  lv_obj_set_parent(sAlertContainer, sCurrentScreen->alert());

  sScreens.pop();
  return sScreens.size();
}

void UiState::react(const system_fsm::KeyLockChanged& ev) {
  sDisplay->SetDisplayOn(!ev.locking);
  sInput->lock(ev.locking);
}

void UiState::react(const system_fsm::BatteryStateChanged& ev) {
  sTopBarModel.battery_state.set(ev.new_state);
}

void UiState::react(const audio::PlaybackStarted&) {}

void UiState::react(const audio::PlaybackFinished&) {}

void UiState::react(const audio::PlaybackUpdate& ev) {}

void UiState::react(const audio::QueueUpdate&) {
  auto& queue = sServices->track_queue();
  sPlaybackModel.current_track.set(queue.current());
}

void UiState::react(const internal::ControlSchemeChanged&) {
  if (!sInput) {
    return;
  }
  sInput->mode(sServices->nvs().PrimaryInput());
}

void UiState::react(const internal::DismissAlerts&) {
  lv_obj_clean(sAlertContainer);
}

namespace states {

void Splash::exit() {
  // buzz a bit to tell the user we're done booting
  events::System().Dispatch(system_fsm::HapticTrigger{
      .effect = drivers::Haptics::Effect::kLongDoubleSharpTick1_100Pct,
  });
}

static std::shared_ptr<Screen> s;
static lv_obj_t* pointer;

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

  sAngleTimer = xTimerCreate("angle", pdMS_TO_TICKS(10), true, NULL,
                             angle_timer_callback);
  xTimerStart(sAngleTimer, portMAX_DELAY);

  // transit<Lua>();
  s = std::make_shared<Screen>();

  sCurrentScreen = s;

  pointer = lv_arc_create(s->content());
  lv_obj_set_size(pointer, 100, 100);
  lv_obj_center(pointer);
  lv_arc_set_range(pointer, 0, 255);
  lv_arc_set_bg_angles(pointer, 0, 360);
  lv_arc_set_rotation(pointer, 270);
}

void Splash::react(const internal::Angle&) {
  if (!s) {
    return;
  }
  auto* wheel = *sServices->touchwheel();
  wheel->Update();
  lv_arc_set_value(
      pointer,
      (255 - static_cast<int>(wheel->GetTouchWheelData().wheel_position)) %
          255);
}

void Splash::react(const system_fsm::StorageMounted&) {}

void Lua::entry() {
  if (!sLua) {
    sAlertTimer = xTimerCreate("ui_alerts", pdMS_TO_TICKS(1000), false, NULL,
                               alert_timer_callback);
    sAlertContainer = lv_obj_create(sCurrentScreen->alert());

    auto bat =
        sServices->battery().State().value_or(battery::Battery::BatteryState{});
    battery_pct_ =
        std::make_shared<lua::Property>(static_cast<int>(bat.percent));
    battery_mv_ =
        std::make_shared<lua::Property>(static_cast<int>(bat.millivolts));
    battery_charging_ = std::make_shared<lua::Property>(bat.is_charging);

    bluetooth_en_ = std::make_shared<lua::Property>(false);

    queue_position_ = std::make_shared<lua::Property>(0);
    queue_size_ = std::make_shared<lua::Property>(0);
    queue_repeat_ = std::make_shared<lua::Property>(false);
    queue_random_ = std::make_shared<lua::Property>(false);

    playback_playing_ = std::make_shared<lua::Property>(
        false, [&](const lua::LuaValue& val) { return SetPlaying(val); });
    playback_track_ = std::make_shared<lua::Property>();
    playback_position_ = std::make_shared<lua::Property>();

    volume_current_pct_ = std::make_shared<lua::Property>(0);
    volume_current_db_ = std::make_shared<lua::Property>(0);

    sLua.reset(lua::LuaThread::Start(*sServices, sCurrentScreen->content()));
    sLua->bridge().AddPropertyModule("power",
                                     {
                                         {"battery_pct", battery_pct_},
                                         {"battery_millivolts", battery_mv_},
                                         {"plugged_in", battery_charging_},
                                     });
    sLua->bridge().AddPropertyModule("bluetooth",
                                     {
                                         {"enabled", bluetooth_en_},
                                         {"connected", bluetooth_en_},
                                     });
    sLua->bridge().AddPropertyModule("playback",
                                     {
                                         {"playing", playback_playing_},
                                         {"track", playback_track_},
                                         {"position", playback_position_},
                                     });
    sLua->bridge().AddPropertyModule("queue", {
                                                  {"position", queue_position_},
                                                  {"size", queue_size_},
                                                  {"replay", queue_repeat_},
                                                  {"random", queue_random_},
                                              });
    sLua->bridge().AddPropertyModule("volume",
                                     {
                                         {"current_pct", volume_current_pct_},
                                         {"current_db", volume_current_db_},
                                     });

    sLua->bridge().AddPropertyModule(
        "backstack",
        {
            {"push", [&](lua_State* s) { return PushLuaScreen(s); }},
            {"pop", [&](lua_State* s) { return PopLuaScreen(s); }},
        });
    sLua->bridge().AddPropertyModule(
        "alerts", {
                      {"show", [&](lua_State* s) { return ShowAlert(s); }},
                      {"hide", [&](lua_State* s) { return HideAlert(s); }},
                  });

    sCurrentScreen.reset();
    sLua->RunScript("/lua/main.lua");
  }
}

auto Lua::PushLuaScreen(lua_State* s) -> int {
  // Ensure the arg looks right before continuing.
  luaL_checktype(s, 1, LUA_TFUNCTION);

  // First, create a new plain old Screen object. We will use its root and
  // group for the Lua screen.
  auto new_screen = std::make_shared<screens::Lua>();

  // Tell lvgl about the new roots.
  luavgl_set_root(s, new_screen->content());
  lv_group_set_default(new_screen->group());

  // Call the constructor for this screen.
  lua_settop(s, 1);  // Make sure the function is actually at top of stack
  lua::CallProtected(s, 0, 1);

  // Store the reference for the table the constructor returned.
  new_screen->SetObjRef(s);

  // Finally, push the now-initialised screen as if it were a regular C++
  // screen.
  PushScreen(new_screen);

  return 0;
}

auto Lua::PopLuaScreen(lua_State* s) -> int {
  PopScreen();
  luavgl_set_root(s, sCurrentScreen->content());
  lv_group_set_default(sCurrentScreen->group());
  return 0;
}

auto Lua::SetPlaying(const lua::LuaValue& val) -> bool {
  bool current_val = std::get<bool>(playback_playing_->Get());
  if (!std::holds_alternative<bool>(val)) {
    return false;
  }
  bool new_val = std::get<bool>(val);
  if (current_val != new_val) {
    events::Audio().Dispatch(audio::TogglePlayPause{});
  }
  return true;
}

auto Lua::ShowAlert(lua_State* s) -> int {
  if (!sCurrentScreen) {
    return 0;
  }
  xTimerReset(sAlertTimer, portMAX_DELAY);
  tinyfsm::FsmList<UiState>::dispatch(internal::DismissAlerts{});

  lv_group_t* prev_group = lv_group_get_default();

  luavgl_set_root(s, sAlertContainer);
  lv_group_t* catchall = lv_group_create();
  lv_group_set_default(catchall);

  // Call the constructor for the alert.
  lua_settop(s, 1);  // Make sure the function is actually at top of stack
  lua::CallProtected(s, 0, 1);

  // Restore the previous group and default object.
  luavgl_set_root(s, sCurrentScreen->content());
  lv_group_set_default(prev_group);

  lv_group_del(catchall);

  return 0;
}

auto Lua::HideAlert(lua_State* s) -> int {
  xTimerStop(sAlertTimer, portMAX_DELAY);
  tinyfsm::FsmList<UiState>::dispatch(internal::DismissAlerts{});
  return 0;
}

auto Lua::SetRandom(const lua::LuaValue& val) -> bool {
  if (!std::holds_alternative<bool>(val)) {
    return false;
  }
  bool b = std::get<bool>(val);
  sServices->track_queue().random(b);
  return true;
}

auto Lua::SetRepeat(const lua::LuaValue& val) -> bool {
  if (!std::holds_alternative<bool>(val)) {
    return false;
  }
  bool b = std::get<bool>(val);
  sServices->track_queue().repeat(b);
  return true;
}

void Lua::exit() {
  lv_group_set_default(NULL);
}

void Lua::react(const OnLuaError& err) {
  ESP_LOGE("lua", "%s", err.message.c_str());
}

void Lua::react(const internal::ShowSettingsPage& ev) {
  PushScreen(std::shared_ptr<Screen>(new screens::Settings(sTopBarModel)));
  transit<Browse>();
}

void Lua::react(const system_fsm::BatteryStateChanged& ev) {
  battery_pct_->Update(static_cast<int>(ev.new_state.percent));
  battery_mv_->Update(static_cast<int>(ev.new_state.millivolts));
}

void Lua::react(const audio::QueueUpdate&) {
  auto& queue = sServices->track_queue();
  queue_size_->Update(static_cast<int>(queue.totalSize()));

  int current_pos = queue.currentPosition();
  if (queue.current()) {
    current_pos++;
  }
  queue_position_->Update(current_pos);
  queue_random_->Update(queue.random());
  queue_repeat_->Update(queue.repeat());
}

void Lua::react(const audio::PlaybackStarted& ev) {
  playback_playing_->Update(true);
}

void Lua::react(const audio::PlaybackUpdate& ev) {
  playback_track_->Update(*ev.track);
  playback_position_->Update(static_cast<int>(ev.seconds_elapsed));
}

void Lua::react(const audio::PlaybackFinished&) {
  playback_playing_->Update(false);
}

void Lua::react(const audio::VolumeChanged& ev) {
  volume_current_pct_->Update(static_cast<int>(ev.percent));
  volume_current_db_->Update(static_cast<int>(ev.db));
}

void Lua::react(const internal::BackPressed& ev) {
  PopLuaScreen(sLua->state());
}

void Browse::entry() {}

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

void Browse::react(const internal::BackPressed& ev) {
  if (PopScreen() == 0) {
    transit<Lua>();
  }
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
  sServices->bg_worker().Dispatch<void>([=]() { db->updateIndexes(); });
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
