/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <memory>
#include <stack>

#include "audio_events.hpp"
#include "battery.hpp"
#include "db_events.hpp"
#include "display.hpp"
#include "encoder_input.hpp"
#include "gpios.hpp"
#include "lua_thread.hpp"
#include "lvgl_task.hpp"
#include "modal.hpp"
#include "nvs.hpp"
#include "property.hpp"
#include "relative_wheel.hpp"
#include "screen.hpp"
#include "service_locator.hpp"
#include "storage.hpp"
#include "system_events.hpp"
#include "tinyfsm.hpp"
#include "touchwheel.hpp"
#include "track.hpp"
#include "track_queue.hpp"
#include "ui_events.hpp"

namespace ui {

class UiState : public tinyfsm::Fsm<UiState> {
 public:
  static auto InitBootSplash(drivers::IGpios&) -> bool;

  virtual ~UiState() {}

  static auto current_screen() -> std::shared_ptr<Screen> {
    return sCurrentScreen;
  }

  virtual void entry() {}
  virtual void exit() {}

  /* Fallback event handler. Does nothing. */
  void react(const tinyfsm::Event& ev) {}

  virtual void react(const OnLuaError&) {}
  virtual void react(const DumpLuaStack&) {}
  virtual void react(const internal::BackPressed&) {}
  virtual void react(const system_fsm::BootComplete&) {}
  virtual void react(const system_fsm::StorageMounted&) {}

  void react(const system_fsm::BatteryStateChanged&);
  void react(const audio::PlaybackStarted&);
  void react(const audio::PlaybackStopped&);
  void react(const audio::PlaybackUpdate&);
  void react(const audio::QueueUpdate&);

  void react(const audio::VolumeChanged&);
  void react(const audio::VolumeBalanceChanged&);
  void react(const audio::VolumeLimitChanged&);

  void react(const system_fsm::KeyLockChanged&);

  void react(const internal::DismissAlerts&);
  void react(const internal::ControlSchemeChanged&);

  void react(const database::event::UpdateStarted&);
  void react(const database::event::UpdateProgress&){};
  void react(const database::event::UpdateFinished&);

  void react(const system_fsm::BluetoothEvent&);

  virtual void react(const internal::ModalCancelPressed&) {
    sCurrentModal.reset();
  }
  virtual void react(const internal::ModalConfirmPressed&) {
    sCurrentModal.reset();
  }

  void react(const internal::ReindexDatabase&){};

 protected:
  void PushScreen(std::shared_ptr<Screen>);
  int PopScreen();

  static std::unique_ptr<UiTask> sTask;
  static std::shared_ptr<system_fsm::ServiceLocator> sServices;
  static std::unique_ptr<drivers::Display> sDisplay;
  static std::shared_ptr<EncoderInput> sInput;

  static std::stack<std::shared_ptr<Screen>> sScreens;
  static std::shared_ptr<Screen> sCurrentScreen;
  static std::shared_ptr<Modal> sCurrentModal;
  static std::shared_ptr<lua::LuaThread> sLua;

  static lua::Property sBatteryPct;
  static lua::Property sBatteryMv;
  static lua::Property sBatteryCharging;

  static lua::Property sBluetoothEnabled;
  static lua::Property sBluetoothConnected;
  static lua::Property sBluetoothPairedDevice;
  static lua::Property sBluetoothDevices;

  static lua::Property sPlaybackPlaying;

  static lua::Property sPlaybackTrack;
  static lua::Property sPlaybackPosition;

  static lua::Property sQueuePosition;
  static lua::Property sQueueSize;
  static lua::Property sQueueReplay;
  static lua::Property sQueueRepeat;
  static lua::Property sQueueRandom;

  static lua::Property sVolumeCurrentPct;
  static lua::Property sVolumeCurrentDb;
  static lua::Property sVolumeLeftBias;
  static lua::Property sVolumeLimit;

  static lua::Property sDisplayBrightness;

  static lua::Property sControlsScheme;
  static lua::Property sScrollSensitivity;

  static lua::Property sDatabaseUpdating;
};

namespace states {

class Splash : public UiState {
 public:
  void exit() override;

  void react(const system_fsm::BootComplete&) override;
  void react(const system_fsm::StorageMounted&) override;

  using UiState::react;
};

class Lua : public UiState {
 public:
  void entry() override;
  void exit() override;

  void react(const OnLuaError&) override;
  void react(const DumpLuaStack&) override;
  void react(const internal::BackPressed&) override;

  using UiState::react;

 private:
  auto PushLuaScreen(lua_State*) -> int;
  auto PopLuaScreen(lua_State*) -> int;

  auto ShowAlert(lua_State*) -> int;
  auto HideAlert(lua_State*) -> int;

  auto SetPlaying(const lua::LuaValue&) -> bool;
  auto SetRandom(const lua::LuaValue&) -> bool;
  auto SetRepeat(const lua::LuaValue&) -> bool;
  auto SetReplay(const lua::LuaValue&) -> bool;

  auto QueueNext(lua_State*) -> int;
  auto QueuePrevious(lua_State*) -> int;
};

}  // namespace states

}  // namespace ui
