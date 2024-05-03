/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ui/ui_fsm.hpp"

#include <memory>
#include <memory_resource>
#include <variant>

#include "FreeRTOSConfig.h"
#include "lvgl.h"

#include "core/lv_group.h"
#include "core/lv_obj.h"
#include "core/lv_obj_tree.h"
#include "esp_heap_caps.h"
#include "esp_spp_api.h"
#include "esp_timer.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "lua.hpp"
#include "luavgl.h"
#include "misc/lv_gc.h"
#include "tinyfsm.hpp"
#include "widgets/lv_label.h"

#include "audio/audio_events.hpp"
#include "audio/audio_fsm.hpp"
#include "audio/track_queue.hpp"
#include "battery/battery.hpp"
#include "database/database.hpp"
#include "database/db_events.hpp"
#include "drivers/bluetooth_types.hpp"
#include "drivers/display.hpp"
#include "drivers/display_init.hpp"
#include "drivers/gpios.hpp"
#include "drivers/haptics.hpp"
#include "drivers/nvs.hpp"
#include "drivers/samd.hpp"
#include "drivers/spiffs.hpp"
#include "drivers/storage.hpp"
#include "drivers/touchwheel.hpp"
#include "events/event_queue.hpp"
#include "input/device_factory.hpp"
#include "input/feedback_haptics.hpp"
#include "input/input_device.hpp"
#include "input/input_touch_wheel.hpp"
#include "input/input_volume_buttons.hpp"
#include "input/lvgl_input_driver.hpp"
#include "lua/lua_registry.hpp"
#include "lua/lua_thread.hpp"
#include "lua/property.hpp"
#include "memory_resource.hpp"
#include "system_fsm/system_events.hpp"
#include "ui/lvgl_task.hpp"
#include "ui/screen.hpp"
#include "ui/screen_lua.hpp"
#include "ui/screen_splash.hpp"
#include "ui/ui_events.hpp"

namespace ui {

[[maybe_unused]] static constexpr char kTag[] = "ui_fsm";

std::unique_ptr<UiTask> UiState::sTask;
std::shared_ptr<system_fsm::ServiceLocator> UiState::sServices;
std::unique_ptr<drivers::Display> UiState::sDisplay;

std::shared_ptr<input::LvglInputDriver> UiState::sInput;
std::unique_ptr<input::DeviceFactory> UiState::sDeviceFactory;

std::stack<std::shared_ptr<Screen>> UiState::sScreens;
std::shared_ptr<Screen> UiState::sCurrentScreen;
std::shared_ptr<Modal> UiState::sCurrentModal;
std::shared_ptr<lua::LuaThread> UiState::sLua;

static TimerHandle_t sAlertTimer;
static lv_obj_t* sAlertContainer;

static void alert_timer_callback(TimerHandle_t timer) {
  events::Ui().Dispatch(internal::DismissAlerts{});
}

lua::Property UiState::sBatteryPct{0};
lua::Property UiState::sBatteryMv{0};
lua::Property UiState::sBatteryCharging{false};

lua::Property UiState::sBluetoothEnabled{
    false, [](const lua::LuaValue& val) {
      if (!std::holds_alternative<bool>(val)) {
        return false;
      }
      if (std::get<bool>(val)) {
        sServices->nvs().OutputMode(drivers::NvsStorage::Output::kBluetooth);
        sServices->bluetooth().Enable();
      } else {
        sServices->nvs().OutputMode(drivers::NvsStorage::Output::kHeadphones);
        sServices->bluetooth().Disable();
      }
      events::Audio().Dispatch(audio::OutputModeChanged{});
      return true;
    }};

lua::Property UiState::sBluetoothConnected{false};
lua::Property UiState::sBluetoothPairedDevice{
    std::monostate{}, [](const lua::LuaValue& val) {
      if (std::holds_alternative<drivers::bluetooth::Device>(val)) {
        auto dev = std::get<drivers::bluetooth::Device>(val);
        sServices->bluetooth().SetPreferredDevice(
            drivers::bluetooth::MacAndName{
                .mac = dev.address,
                .name = {dev.name.data(), dev.name.size()},
            });
      }
      return false;
    }};
lua::Property UiState::sBluetoothDevices{
    std::vector<drivers::bluetooth::Device>{}};

lua::Property UiState::sPlaybackPlaying{
    false, [](const lua::LuaValue& val) {
      if (!std::holds_alternative<bool>(val)) {
        return false;
      }
      bool new_val = std::get<bool>(val);
      events::Audio().Dispatch(audio::TogglePlayPause{.set_to = new_val});
      return true;
    }};

lua::Property UiState::sPlaybackTrack{};
lua::Property UiState::sPlaybackPosition{
    0, [](const lua::LuaValue& val) {
      int current_val = std::get<int>(sPlaybackPosition.get());
      if (!std::holds_alternative<int>(val)) {
        return false;
      }
      int new_val = std::get<int>(val);
      if (current_val != new_val) {
        auto track = sPlaybackTrack.get();
        if (!std::holds_alternative<audio::TrackInfo>(track)) {
          return false;
        }
        events::Audio().Dispatch(audio::SetTrack{
            .new_track = std::get<audio::TrackInfo>(track).uri,
            .seek_to_second = (uint32_t)new_val,
        });
      }
      return true;
    }};

lua::Property UiState::sQueuePosition{0};
lua::Property UiState::sQueueSize{0};
lua::Property UiState::sQueueRepeat{false, [](const lua::LuaValue& val) {
                                      if (!std::holds_alternative<bool>(val)) {
                                        return false;
                                      }
                                      bool new_val = std::get<bool>(val);
                                      sServices->track_queue().repeat(new_val);
                                      return true;
                                    }};
lua::Property UiState::sQueueReplay{false, [](const lua::LuaValue& val) {
                                      if (!std::holds_alternative<bool>(val)) {
                                        return false;
                                      }
                                      bool new_val = std::get<bool>(val);
                                      sServices->track_queue().replay(new_val);
                                      return true;
                                    }};
lua::Property UiState::sQueueRandom{false, [](const lua::LuaValue& val) {
                                      if (!std::holds_alternative<bool>(val)) {
                                        return false;
                                      }
                                      bool new_val = std::get<bool>(val);
                                      sServices->track_queue().random(new_val);
                                      return true;
                                    }};

lua::Property UiState::sVolumeCurrentPct{
    0, [](const lua::LuaValue& val) {
      if (!std::holds_alternative<int>(val)) {
        return false;
      }
      events::Audio().Dispatch(audio::SetVolume{
          .percent = std::get<int>(val),
          .db = {},
      });
      return true;
    }};
lua::Property UiState::sVolumeCurrentDb{
    0, [](const lua::LuaValue& val) {
      if (!std::holds_alternative<int>(val)) {
        return false;
      }
      events::Audio().Dispatch(audio::SetVolume{
          .percent = {},
          .db = std::get<int>(val),
      });
      return true;
    }};
lua::Property UiState::sVolumeLeftBias{
    0, [](const lua::LuaValue& val) {
      if (!std::holds_alternative<int>(val)) {
        return false;
      }
      events::Audio().Dispatch(audio::SetVolumeBalance{
          .left_bias = std::get<int>(val),
      });
      return true;
    }};
lua::Property UiState::sVolumeLimit{
    0, [](const lua::LuaValue& val) {
      if (!std::holds_alternative<int>(val)) {
        return false;
      }
      int limit = std::get<int>(val);
      events::Audio().Dispatch(audio::SetVolumeLimit{
          .limit_db = limit,
      });
      return true;
    }};

lua::Property UiState::sDisplayBrightness{
    0, [](const lua::LuaValue& val) {
      std::optional<int> brightness = 0;
      std::visit(
          [&](auto&& v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, int>) {
              brightness = v;
            }
          },
          val);
      if (!brightness) {
        return false;
      }
      sDisplay->SetBrightness(*brightness);
      sServices->nvs().ScreenBrightness(*brightness);
      return true;
    }};

lua::Property UiState::sLockSwitch{false};

lua::Property UiState::sDatabaseUpdating{false};
lua::Property UiState::sDatabaseAutoUpdate{
    false, [](const lua::LuaValue& val) {
      if (!std::holds_alternative<bool>(val)) {
        return false;
      }
      sServices->nvs().DbAutoIndex(std::get<bool>(val));
      return true;
    }};

lua::Property UiState::sSdMounted{false};

lua::Property UiState::sUsbMassStorageEnabled{
    false, [](const lua::LuaValue& val) {
      if (!std::holds_alternative<bool>(val)) {
        return false;
      }
      bool enable = std::get<bool>(val);
      // FIXME: Check for system busy.
      events::System().Dispatch(system_fsm::SamdUsbMscChanged{.en = enable});
      return true;
    }};

lua::Property UiState::sUsbMassStorageBusy{false};

auto UiState::InitBootSplash(drivers::IGpios& gpios, drivers::NvsStorage& nvs)
    -> bool {
  events::Ui().Dispatch(internal::InitDisplay{
      .gpios = gpios,
      .nvs = nvs,
  });
  sTask.reset(UiTask::Start());
  return true;
}

void UiState::react(const internal::InitDisplay& ev) {
  // Init LVGL first, since the display driver registers itself with LVGL.
  lv_init();

  drivers::displays::InitialisationData init_data = drivers::displays::kST7735R;

  // HACK: correct the display size for our prototypes.
  // nvs.DisplaySize({161, 130});

  auto actual_size = ev.nvs.DisplaySize();
  init_data.width = actual_size.first.value_or(init_data.width);
  init_data.height = actual_size.second.value_or(init_data.height);
  sDisplay.reset(drivers::Display::Create(ev.gpios, init_data));

  sCurrentScreen.reset(new screens::Splash());

  // Display will only actually come on after LVGL finishes its first flush.
  sDisplay->SetDisplayOn(!ev.gpios.IsLocked());
}

void UiState::PushScreen(std::shared_ptr<Screen> screen) {
  lv_obj_set_parent(sAlertContainer, screen->alert());

  if (sCurrentScreen) {
    sCurrentScreen->onHidden();
    sScreens.push(sCurrentScreen);
  }
  sCurrentScreen = screen;
  sCurrentScreen->onShown();
}

int UiState::PopScreen() {
  if (sScreens.empty()) {
    return 0;
  }
  lv_obj_set_parent(sAlertContainer, sScreens.top()->alert());

  sCurrentScreen->onHidden();

  sCurrentScreen = sScreens.top();
  sScreens.pop();

  sCurrentScreen->onShown();

  return sScreens.size();
}

void UiState::react(const system_fsm::KeyLockChanged& ev) {
  sDisplay->SetDisplayOn(!ev.locking);
  sInput->lock(ev.locking);
  sLockSwitch.setDirect(ev.locking);
}

void UiState::react(const system_fsm::SamdUsbStatusChanged& ev) {
  sUsbMassStorageBusy.setDirect(ev.new_status ==
                                drivers::Samd::UsbStatus::kAttachedBusy);
}

void UiState::react(const system_fsm::SdStateChanged&) {
  sSdMounted.setDirect(sServices->sd() == drivers::SdState::kMounted);
}

void UiState::react(const database::event::UpdateStarted&) {
  sDatabaseUpdating.setDirect(true);
}

void UiState::react(const database::event::UpdateFinished&) {
  sDatabaseUpdating.setDirect(false);
}

void UiState::react(const internal::DismissAlerts&) {
  lv_obj_clean(sAlertContainer);
}

void UiState::react(const system_fsm::BatteryStateChanged& ev) {
  sBatteryPct.setDirect(static_cast<int>(ev.new_state.percent));
  sBatteryMv.setDirect(static_cast<int>(ev.new_state.millivolts));
  sBatteryCharging.setDirect(ev.new_state.is_charging);
}

void UiState::react(const audio::QueueUpdate&) {
  auto& queue = sServices->track_queue();
  sQueueSize.setDirect(static_cast<int>(queue.totalSize()));

  int current_pos = queue.currentPosition();
  if (queue.current()) {
    current_pos++;
  }
  sQueuePosition.setDirect(current_pos);
  sQueueRandom.setDirect(queue.random());
  sQueueRepeat.setDirect(queue.repeat());
  sQueueReplay.setDirect(queue.replay());
}

void UiState::react(const audio::PlaybackUpdate& ev) {
  if (ev.current_track) {
    sPlaybackTrack.setDirect(*ev.current_track);
  } else {
    sPlaybackTrack.setDirect(std::monostate{});
  }
  sPlaybackPlaying.setDirect(!ev.paused);
  sPlaybackPosition.setDirect(static_cast<int>(ev.track_position.value_or(0)));
}

void UiState::react(const audio::VolumeChanged& ev) {
  sVolumeCurrentPct.setDirect(static_cast<int>(ev.percent));
  sVolumeCurrentDb.setDirect(static_cast<int>(ev.db));
}

void UiState::react(const audio::VolumeBalanceChanged& ev) {
  sVolumeLeftBias.setDirect(ev.left_bias);
}

void UiState::react(const audio::VolumeLimitChanged& ev) {
  sVolumeLimit.setDirect(ev.new_limit_db);
}

void UiState::react(const system_fsm::BluetoothEvent& ev) {
  auto bt = sServices->bluetooth();
  auto dev = bt.ConnectedDevice();
  switch (ev.event) {
    case drivers::bluetooth::Event::kKnownDevicesChanged:
      sBluetoothDevices.setDirect(bt.KnownDevices());
      break;
    case drivers::bluetooth::Event::kConnectionStateChanged:
      sBluetoothConnected.setDirect(bt.IsConnected());
      if (dev) {
        sBluetoothPairedDevice.setDirect(drivers::bluetooth::Device{
            .address = dev->mac,
            .name = {dev->name.data(), dev->name.size()},
            .class_of_device = 0,
            .signal_strength = 0,
        });
      } else {
        sBluetoothPairedDevice.setDirect(std::monostate{});
      }
      break;
    case drivers::bluetooth::Event::kPreferredDeviceChanged:
      break;
  }
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

  int brightness = sServices->nvs().ScreenBrightness();
  sDisplayBrightness.setDirect(brightness);
  sDisplay->SetBrightness(brightness);

  sDeviceFactory = std::make_unique<input::DeviceFactory>(sServices);
  sInput = std::make_shared<input::LvglInputDriver>(sServices->nvs(),
                                                    *sDeviceFactory);

  sTask->input(sInput);
}

void Splash::react(const system_fsm::SdStateChanged& ev) {
  UiState::react(ev);
  transit<Lua>();
}

void Lua::entry() {
  if (!sLua) {
    sAlertTimer = xTimerCreate("ui_alerts", pdMS_TO_TICKS(1000), false, NULL,
                               alert_timer_callback);
    sAlertContainer = lv_obj_create(sCurrentScreen->alert());
    lv_obj_set_style_bg_opa(sAlertContainer, LV_OPA_TRANSP, 0);

    auto& registry = lua::Registry::instance(*sServices);
    sLua = registry.uiThread();
    registry.AddPropertyModule("power", {
                                            {"battery_pct", &sBatteryPct},
                                            {"battery_millivolts", &sBatteryMv},
                                            {"plugged_in", &sBatteryCharging},
                                        });
    registry.AddPropertyModule("bluetooth",
                               {
                                   {"enabled", &sBluetoothEnabled},
                                   {"connected", &sBluetoothConnected},
                                   {"paired_device", &sBluetoothPairedDevice},
                                   {"devices", &sBluetoothDevices},
                               });
    registry.AddPropertyModule("playback", {
                                               {"playing", &sPlaybackPlaying},
                                               {"track", &sPlaybackTrack},
                                               {"position", &sPlaybackPosition},
                                           });
    registry.AddPropertyModule(
        "queue",
        {
            {"next", [&](lua_State* s) { return QueueNext(s); }},
            {"previous", [&](lua_State* s) { return QueuePrevious(s); }},
            {"position", &sQueuePosition},
            {"size", &sQueueSize},
            {"replay", &sQueueReplay},
            {"repeat_track", &sQueueRepeat},
            {"random", &sQueueRandom},
        });
    registry.AddPropertyModule("volume",
                               {
                                   {"current_pct", &sVolumeCurrentPct},
                                   {"current_db", &sVolumeCurrentDb},
                                   {"left_bias", &sVolumeLeftBias},
                                   {"limit_db", &sVolumeLimit},
                               });

    registry.AddPropertyModule("display",
                               {
                                   {"brightness", &sDisplayBrightness},
                               });

    registry.AddPropertyModule(
        "controls",
        {
            {"scheme", &sInput->mode()},
            {"lock_switch", &sLockSwitch},
            {"hooks", [&](lua_State* L) { return sInput->pushHooks(L); }},
        });

    if (sDeviceFactory->touch_wheel()) {
      registry.AddPropertyModule(
          "controls", {{"scroll_sensitivity",
                        &sDeviceFactory->touch_wheel()->sensitivity()}});
    }

    registry.AddPropertyModule(
        "backstack",
        {
            {"push", [&](lua_State* s) { return PushLuaScreen(s); }},
            {"pop", [&](lua_State* s) { return PopLuaScreen(s); }},
            {"reset", [&](lua_State* s) { return ResetLuaScreen(s); }},
        });
    registry.AddPropertyModule(
        "alerts", {
                      {"show", [&](lua_State* s) { return ShowAlert(s); }},
                      {"hide", [&](lua_State* s) { return HideAlert(s); }},
                  });

    registry.AddPropertyModule(
        "time", {
                    {"ticks", [&](lua_State* s) { return Ticks(s); }},
                });
    registry.AddPropertyModule("database",
                               {
                                   {"updating", &sDatabaseUpdating},
                                   {"auto_update", &sDatabaseAutoUpdate},
                               });
    registry.AddPropertyModule("sd_card", {
                                              {"mounted", &sSdMounted},
                                          });
    registry.AddPropertyModule("usb",
                               {
                                   {"msc_enabled", &sUsbMassStorageEnabled},
                                   {"msc_busy", &sUsbMassStorageBusy},
                               });

    sDatabaseAutoUpdate.setDirect(sServices->nvs().DbAutoIndex());

    auto bt = sServices->bluetooth();
    sBluetoothEnabled.setDirect(bt.IsEnabled());
    sBluetoothConnected.setDirect(bt.IsConnected());
    sBluetoothDevices.setDirect(bt.KnownDevices());

    sCurrentScreen.reset();
    if (sServices->sd() == drivers::SdState::kMounted) {
      sLua->RunScript("/sdcard/config.lua");
    }
    sLua->RunScript("/lua/main.lua");
  }
}

auto Lua::PushLuaScreen(lua_State* s) -> int {
  // Ensure the arg looks right before continuing.
  luaL_checktype(s, 1, LUA_TTABLE);

  // First, create a new plain old Screen object. We will use its root and
  // group for the Lua screen. Allocate it in external ram so that arbitrarily
  // deep screen stacks don't cause too much memory pressure.
  auto new_screen =
      std::allocate_shared<screens::Lua,
                           std::pmr::polymorphic_allocator<screens::Lua>>(
          &memory::kSpiRamResource);

  // Tell lvgl about the new roots.
  luavgl_set_root(s, new_screen->content());
  lv_group_set_default(new_screen->group());

  // Call the constructor for this screen.
  // lua_settop(s, 1);  // Make sure the screen is actually at top of stack
  lua_pushliteral(s, "createUi");
  if (lua_gettable(s, 1) == LUA_TFUNCTION) {
    lua_pushvalue(s, 1);
    lua::CallProtected(s, 1, 0);
  }

  // Store the reference for this screen's table.
  lua_settop(s, 1);
  new_screen->SetObjRef(s);

  // Finally, push the now-initialised screen as if it were a regular C++
  // screen.
  PushScreen(new_screen);

  return 0;
}

auto Lua::PopLuaScreen(lua_State* s) -> int {
  if (!sCurrentScreen->canPop()) {
    return 0;
  }
  PopScreen();
  luavgl_set_root(s, sCurrentScreen->content());
  lv_group_set_default(sCurrentScreen->group());
  return 0;
}

auto Lua::ResetLuaScreen(lua_State* s) -> int {
  if (sCurrentScreen) {
    if (!sCurrentScreen->canPop()) {
      ESP_LOGW(kTag, "ignoring reset as popping is blocked");
      return 0;
    }
    sCurrentScreen->onHidden();
  }
  while (!sScreens.empty()) {
    sScreens.pop();
  }
  return PushLuaScreen(s);
}

auto Lua::QueueNext(lua_State*) -> int {
  sServices->track_queue().next();
  return 0;
}

auto Lua::QueuePrevious(lua_State*) -> int {
  sServices->track_queue().previous();
  return 0;
}

auto Lua::Ticks(lua_State* s) -> int {
  lua_pushinteger(s, esp_timer_get_time() / 1000);
  return 1;
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

auto Lua::SetReplay(const lua::LuaValue& val) -> bool {
  if (!std::holds_alternative<bool>(val)) {
    return false;
  }
  bool b = std::get<bool>(val);
  sServices->track_queue().replay(b);
  return true;
}

void Lua::exit() {
  lv_group_set_default(NULL);
}

void Lua::react(const OnLuaError& err) {
  ESP_LOGE("lua", "%s", err.message.c_str());
}

void Lua::react(const DumpLuaStack& ev) {
  sLua->DumpStack();
}

void Lua::react(const internal::BackPressed& ev) {
  PopLuaScreen(sLua->state());
}

}  // namespace states
}  // namespace ui

FSM_INITIAL_STATE(ui::UiState, ui::states::Splash)
