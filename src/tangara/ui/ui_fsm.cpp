/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ui/ui_fsm.hpp"
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <memory_resource>
#include <variant>

#include "FreeRTOSConfig.h"
#include "draw/lv_draw_buf.h"
#include "drivers/bluetooth.hpp"
#include "lauxlib.h"
#include "lua.h"
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
#include "misc/lv_color.h"
#include "misc/lv_utils.h"
#include "others/snapshot/lv_snapshot.h"
#include "tick/lv_tick.h"
#include "tinyfsm.hpp"

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
#include "ui/screenshot.hpp"
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
std::shared_ptr<lua::LuaThread> UiState::sLua;

static TimerHandle_t sAlertTimer;
static lv_obj_t* sAlertContainer;

static void alert_timer_callback(TimerHandle_t timer) {
  events::Ui().Dispatch(internal::DismissAlerts{});
}

static auto lvgl_tick_cb() -> uint32_t {
  return esp_timer_get_time() / 1000;
}

static auto lvgl_delay_cb(uint32_t ms) -> void {
  vTaskDelay(pdMS_TO_TICKS(ms));
}

lua::Property UiState::sBatteryPct{0};
lua::Property UiState::sBatteryMv{0};
lua::Property UiState::sBatteryCharging{false};
lua::Property UiState::sPowerChargeState{"unknown"};
lua::Property UiState::sPowerFastChargeEnabled{
    false, [](const lua::LuaValue& val) {
      if (!std::holds_alternative<bool>(val)) {
        return false;
      }
      sServices->samd().SetFastChargeEnabled(std::get<bool>(val));
      return true;
    }};

lua::Property UiState::sBluetoothEnabled{
    false, [](const lua::LuaValue& val) {
      if (!std::holds_alternative<bool>(val)) {
        return false;
      }
      // Note we always write the OutputMode NVS change before actually
      // modifying the peripheral. We do this because ESP-IDF's Bluetooth stack
      // breaks in surprising ways when repeatedly initialised/uninitialised.
      if (std::get<bool>(val)) {
        sServices->nvs().OutputMode(drivers::NvsStorage::Output::kBluetooth);
        sServices->bluetooth().enable(true);
      } else {
        sServices->nvs().OutputMode(drivers::NvsStorage::Output::kHeadphones);
        sServices->bluetooth().enable(false);
      }
      events::Audio().Dispatch(audio::OutputModeChanged{});
      return true;
    }};

lua::Property UiState::sBluetoothConnecting{false};
lua::Property UiState::sBluetoothConnected{false};

lua::Property UiState::sBluetoothDiscovering{
    false, [](const lua::LuaValue& val) {
      if (!std::holds_alternative<bool>(val)) {
        return false;
      }
      // Note we always write the OutputMode NVS change before actually
      // modifying the peripheral. We do this because ESP-IDF's Bluetooth stack
      // breaks in surprising ways when repeatedly initialised/uninitialised.
      if (std::get<bool>(val)) {
        sServices->bluetooth().discoveryEnabled(true);
      } else {
        sServices->bluetooth().discoveryEnabled(false);
      }
      return true;
    }};

lua::Property UiState::sBluetoothPairedDevice{
    std::monostate{}, [](const lua::LuaValue& val) {
      if (std::holds_alternative<drivers::bluetooth::MacAndName>(val)) {
        auto dev = std::get<drivers::bluetooth::MacAndName>(val);
        sServices->bluetooth().pairedDevice(dev);
      } else if (std::holds_alternative<std::monostate>(val)) {
        sServices->bluetooth().pairedDevice({});
      } else {
        // Don't accept any other types.
        return false;
      }
      return true;
    }};

lua::Property UiState::sBluetoothKnownDevices{
    std::vector<drivers::bluetooth::MacAndName>{}};
lua::Property UiState::sBluetoothDiscoveredDevices{
    std::vector<drivers::bluetooth::MacAndName>{}};

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

lua::Property UiState::sQueuePosition{0, [](const lua::LuaValue& val){
                                      if (!std::holds_alternative<int>(val)) {
                                        return false;
                                      }
                                      int new_val = std::get<int>(val);
                                      // val-1 because Lua uses 1-based indexing
                                      return sServices->track_queue().currentPosition(new_val-1);
                                    }};
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
lua::Property UiState::sQueueLoading{false};

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
  lv_tick_set_cb(lvgl_tick_cb);
  lv_delay_set_cb(lvgl_delay_cb);

  drivers::displays::InitialisationData init_data = drivers::displays::kST7735R;

  // HACK: correct the display size for our prototypes.
  // ev.nvs.DisplaySize({161, 130});

  auto actual_size = ev.nvs.DisplaySize();
  init_data.width = actual_size.first.value_or(init_data.width);
  init_data.height = actual_size.second.value_or(init_data.height);
  sDisplay.reset(drivers::Display::Create(ev.gpios, init_data));

  sCurrentScreen.reset(new screens::Splash());

  // Display will only actually come on after LVGL finishes its first flush.
  sDisplay->SetDisplayOn(!ev.gpios.IsLocked());
}

void UiState::PushScreen(std::shared_ptr<Screen> screen, bool replace) {
  lv_obj_set_parent(sAlertContainer, screen->alert());

  if (sCurrentScreen) {
    sCurrentScreen->onHidden();
    if (!replace) {
      sScreens.push(sCurrentScreen);
    }
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

void UiState::react(const Screenshot& ev) {
  if (!sCurrentScreen) {
    return;
  }
  SaveScreenshot(sCurrentScreen->root(), ev.filename);
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
  sPowerChargeState.setDirect(
      drivers::Samd::chargeStatusToString(ev.new_state.raw_status));

  // FIXME: Avoid calling these event handlers before boot.
  if (sServices) {
    sPowerFastChargeEnabled.setDirect(sServices->nvs().FastCharge());
  }
}

void UiState::react(const audio::QueueUpdate& update) {
  auto& queue = sServices->track_queue();
  auto queue_size = queue.totalSize();
  sQueueSize.setDirect(static_cast<int>(queue_size));

  int current_pos = queue.currentPosition();
  // If there is nothing in the queue, the position should be 0, otherwise, add
  // one because lua
  if (queue_size > 0) {
    current_pos++;
  }
  if (current_pos > queue_size) {
    current_pos = queue_size;
  }
  sQueuePosition.setDirect(current_pos);
  sQueueRandom.setDirect(queue.random());
  sQueueRepeat.setDirect(queue.repeat());
  sQueueReplay.setDirect(queue.replay());

  if (update.reason == audio::QueueUpdate::Reason::kBulkLoadingUpdate) {
    sQueueLoading.setDirect(true);
  } else {
    sQueueLoading.setDirect(false);
  }
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

void UiState::react(const audio::RemoteVolumeChanged& ev) {
  // TODO: Show dialog
}

void UiState::react(const audio::VolumeBalanceChanged& ev) {
  sVolumeLeftBias.setDirect(ev.left_bias);
}

void UiState::react(const audio::VolumeLimitChanged& ev) {
  sVolumeLimit.setDirect(ev.new_limit_db);
}

void UiState::react(const system_fsm::BluetoothEvent& ev) {
  using drivers::bluetooth::SimpleEvent;
  using ConnectionState = drivers::Bluetooth::ConnectionState;
  ConnectionState state;
  auto bt = sServices->bluetooth();

  std::optional<drivers::bluetooth::MacAndName> dev;
  std::vector<drivers::bluetooth::MacAndName> devs;

  if (std::holds_alternative<SimpleEvent>(ev.event)) {
    switch (std::get<SimpleEvent>(ev.event)) {
      case SimpleEvent::kPlayPause:
        events::Audio().Dispatch(audio::TogglePlayPause{});
        break;
      case SimpleEvent::kStop:
        events::Audio().Dispatch(audio::TogglePlayPause{.set_to = false});
        break;
      case SimpleEvent::kMute:
        break;
      case SimpleEvent::kVolUp:
        break;
      case SimpleEvent::kVolDown:
        break;
      case SimpleEvent::kForward:
        sServices->track_queue().next();
        break;
      case SimpleEvent::kBackward:
        sServices->track_queue().previous();
        break;
      case SimpleEvent::kRewind:
        break;
      case SimpleEvent::kFastForward:
        break;
      case SimpleEvent::kConnectionStateChanged:
        state = bt.connectionState();
        sBluetoothConnected.setDirect(state == ConnectionState::kConnected);
        sBluetoothConnecting.setDirect(state == ConnectionState::kConnecting);
        break;
      case SimpleEvent::kPairedDeviceChanged:
        dev = bt.pairedDevice();
        if (dev) {
          sBluetoothPairedDevice.setDirect(*dev);
        } else {
          sBluetoothPairedDevice.setDirect(std::monostate{});
        }
        break;
      case SimpleEvent::kKnownDevicesChanged:
        sBluetoothKnownDevices.setDirect(bt.knownDevices());
        break;
      case SimpleEvent::kDiscoveryChanged:
        sBluetoothDiscovering.setDirect(bt.discoveryEnabled());
        // Dump the old list of discovered devices when discovery is toggled.
        sBluetoothDiscoveredDevices.setDirect(bt.discoveredDevices());
        break;
      case SimpleEvent::kDeviceDiscovered:
        sBluetoothDiscoveredDevices.setDirect(bt.discoveredDevices());
        break;
      default:
        break;
    }
  } else if (std::holds_alternative<drivers::bluetooth::RemoteVolumeChanged>(
                 ev.event)) {
    // TODO: Do something with this (ie, bt volume alert)
    ESP_LOGI(
        kTag, "Recieved volume changed event with new volume: %d",
        std::get<drivers::bluetooth::RemoteVolumeChanged>(ev.event).new_vol);
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

  lv_theme_t* base_theme = lv_theme_simple_init(NULL);
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
    registry.AddPropertyModule("power",
                               {
                                   {"battery_pct", &sBatteryPct},
                                   {"battery_millivolts", &sBatteryMv},
                                   {"plugged_in", &sBatteryCharging},
                                   {"charge_state", &sPowerChargeState},
                                   {"fast_charge", &sPowerFastChargeEnabled},
                               });
    registry.AddPropertyModule(
        "bluetooth", {
                         {"enabled", &sBluetoothEnabled},
                         {"connected", &sBluetoothConnected},
                         {"connecting", &sBluetoothConnecting},
                         {"discovering", &sBluetoothDiscovering},
                         {"paired_device", &sBluetoothPairedDevice},
                         {"discovered_devices", &sBluetoothDiscoveredDevices},
                         {"known_devices", &sBluetoothKnownDevices},
                         {"enable", 
                            [&](lua_State* s) {
                              sBluetoothEnabled.set(true);
                              return 0;
                         }},
                         {"disable", 
                            [&](lua_State* s) {
                              sBluetoothEnabled.set(false);
                              return 0;
                         }},
                     });
    registry.AddPropertyModule(
        "playback",
        {
            {"playing", &sPlaybackPlaying},
            {"track", &sPlaybackTrack},
            {"position", &sPlaybackPosition},
            {"is_playable",
             [&](lua_State* s) {
               size_t len;
               const char* path = luaL_checklstring(s, 1, &len);
               auto res = sServices->tag_parser().ReadAndParseTags({path, len});
               if (res) {
                 lua_pushboolean(s, true);
               } else {
                 lua_pushboolean(s, false);
               }
               return 1;
             }},
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
            {"loading", &sQueueLoading},
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
            {"push", [&](lua_State* s) { return PushLuaScreen(s, false); }},
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
    sBluetoothEnabled.setDirect(bt.enabled());
    auto paired = bt.pairedDevice();
    if (paired) {
      sBluetoothPairedDevice.setDirect(*paired);
    }
    sBluetoothKnownDevices.setDirect(bt.knownDevices());

    sPowerFastChargeEnabled.setDirect(sServices->nvs().FastCharge());

    if (sServices->sd() == drivers::SdState::kMounted) {
      sLua->RunScript("/sd/config.lua");
    }
    sLua->RunScript("/lua/main.lua");
  }
}

auto Lua::PushLuaScreen(lua_State* s, bool replace) -> int {
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
  lua_pushliteral(s, "create_ui");
  if (lua_gettable(s, 1) == LUA_TFUNCTION) {
    lua_pushvalue(s, 1);
    lua::CallProtected(s, 1, 0);
  }

  // Store the reference for this screen's table.
  lua_settop(s, 1);
  new_screen->SetObjRef(s);

  // Finally, push the now-initialised screen as if it were a regular C++
  // screen.
  PushScreen(new_screen, replace);

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
  }
  while (!sScreens.empty()) {
    sScreens.pop();
  }
  return PushLuaScreen(s, true);
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
