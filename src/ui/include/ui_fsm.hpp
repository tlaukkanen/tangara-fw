/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <sys/_stdint.h>
#include <memory>
#include <stack>

#include "audio_events.hpp"
#include "battery.hpp"
#include "bindey/property.h"
#include "db_events.hpp"
#include "gpios.hpp"
#include "lua_thread.hpp"
#include "lvgl_task.hpp"
#include "model_playback.hpp"
#include "model_top_bar.hpp"
#include "nvs.hpp"
#include "property.hpp"
#include "relative_wheel.hpp"
#include "screen_playing.hpp"
#include "screen_settings.hpp"
#include "service_locator.hpp"
#include "tinyfsm.hpp"

#include "display.hpp"
#include "encoder_input.hpp"
#include "modal.hpp"
#include "screen.hpp"
#include "storage.hpp"
#include "system_events.hpp"
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

  virtual void react(const system_fsm::BatteryStateChanged&);
  virtual void react(const audio::PlaybackStarted&);
  virtual void react(const audio::PlaybackFinished&);
  void react(const audio::PlaybackUpdate&);
  void react(const audio::QueueUpdate&);

  virtual void react(const system_fsm::KeyLockChanged&);

  virtual void react(const internal::RecordSelected&) {}
  virtual void react(const internal::IndexSelected&) {}
  virtual void react(const internal::BackPressed&) {}
  virtual void react(const internal::ShowNowPlaying&){};
  virtual void react(const internal::ShowSettingsPage&){};
  virtual void react(const internal::ModalCancelPressed&) {
    sCurrentModal.reset();
  }
  virtual void react(const internal::ModalConfirmPressed&) {
    sCurrentModal.reset();
  }
  virtual void react(const internal::OnboardingNavigate&) {}
  void react(const internal::ControlSchemeChanged&);
  virtual void react(const internal::ReindexDatabase&){};

  virtual void react(const database::event::UpdateStarted&){};
  virtual void react(const database::event::UpdateProgress&){};
  virtual void react(const database::event::UpdateFinished&){};

  virtual void react(const system_fsm::DisplayReady&) {}
  virtual void react(const system_fsm::BootComplete&) {}
  virtual void react(const system_fsm::StorageMounted&) {}
  virtual void react(const system_fsm::BluetoothDevicesChanged&) {}

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

  static std::weak_ptr<screens::Bluetooth> bluetooth_screen_;

  static models::Playback sPlaybackModel;
  static models::TopBar sTopBarModel;
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

  void react(const internal::IndexSelected&) override;
  void react(const internal::ShowNowPlaying&) override;
  void react(const internal::ShowSettingsPage&) override;

  void react(const system_fsm::BatteryStateChanged&) override;
  void react(const audio::PlaybackStarted&) override;
  void react(const audio::PlaybackFinished&) override;

  using UiState::react;

 private:
  auto PushLuaScreen(lua_State*) -> int;
  auto PopLuaScreen(lua_State*) -> int;

  std::shared_ptr<lua::Property> battery_pct_;
  std::shared_ptr<lua::Property> battery_mv_;
  std::shared_ptr<lua::Property> battery_charging_;
  std::shared_ptr<lua::Property> bluetooth_en_;
  std::shared_ptr<lua::Property> playback_playing_;
  std::shared_ptr<lua::Property> playback_track_;
};

class Onboarding : public UiState {
 public:
  void entry() override;

  void react(const internal::OnboardingNavigate&) override;

  using UiState::react;

 private:
  uint8_t progress_;
  bool has_formatted_;
};

class Browse : public UiState {
 public:
  void entry() override;

  void react(const internal::RecordSelected&) override;
  void react(const internal::BackPressed&) override;

  void react(const internal::ShowNowPlaying&) override;
  void react(const internal::ShowSettingsPage&) override;
  void react(const internal::ReindexDatabase&) override;

  void react(const system_fsm::BluetoothDevicesChanged&) override;

  using UiState::react;
};

class Playing : public UiState {
 public:
  void entry() override;
  void exit() override;

  void react(const internal::BackPressed&) override;

  using UiState::react;
};

class Indexing : public UiState {
 public:
  void entry() override;
  void exit() override;

  void react(const database::event::UpdateStarted&) override;
  void react(const database::event::UpdateProgress&) override;
  void react(const database::event::UpdateFinished&) override;

  using UiState::react;
};

class FatalError : public UiState {};

}  // namespace states

}  // namespace ui
