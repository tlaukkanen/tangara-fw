/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <memory>
#include <stack>

#include "audio_events.hpp"
#include "battery.hpp"
#include "gpios.hpp"
#include "lvgl_task.hpp"
#include "nvs.hpp"
#include "relative_wheel.hpp"
#include "screen_playing.hpp"
#include "service_locator.hpp"
#include "tinyfsm.hpp"

#include "display.hpp"
#include "modal.hpp"
#include "screen.hpp"
#include "storage.hpp"
#include "system_events.hpp"
#include "touchwheel.hpp"
#include "track_queue.hpp"
#include "ui_events.hpp"
#include "wheel_encoder.hpp"

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
  virtual void react(const audio::PlaybackUpdate&) {}
  virtual void react(const audio::QueueUpdate&) {}

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

  virtual void react(const system_fsm::DisplayReady&) {}
  virtual void react(const system_fsm::BootComplete&) {}
  virtual void react(const system_fsm::StorageMounted&) {}

 protected:
  void PushScreen(std::shared_ptr<Screen>);
  void PopScreen();
  void UpdateTopBar();

  static std::unique_ptr<UiTask> sTask;
  static std::shared_ptr<system_fsm::ServiceLocator> sServices;
  static std::unique_ptr<drivers::Display> sDisplay;
  static std::shared_ptr<TouchWheelEncoder> sEncoder;

  static std::stack<std::shared_ptr<Screen>> sScreens;
  static std::shared_ptr<Screen> sCurrentScreen;
  static std::shared_ptr<Modal> sCurrentModal;
};

namespace states {

class Splash : public UiState {
 public:
  void exit() override;
  void react(const system_fsm::BootComplete&) override;
  void react(const system_fsm::BatteryStateChanged&) override{};
  using UiState::react;
};

class Onboarding : public UiState {
 public:
  void entry() override;

  using UiState::react;

 private:
  uint8_t progress_;
};

class Browse : public UiState {
  void entry() override;

  void react(const internal::RecordSelected&) override;
  void react(const internal::IndexSelected&) override;
  void react(const internal::BackPressed&) override;

  void react(const internal::ShowNowPlaying&) override;
  void react(const internal::ShowSettingsPage&) override;

  void react(const system_fsm::StorageMounted&) override;
  using UiState::react;
};

class Playing : public UiState {
  void entry() override;
  void exit() override;

  void react(const internal::BackPressed&) override;

  void react(const audio::PlaybackStarted&) override;
  void react(const audio::PlaybackUpdate&) override;
  void react(const audio::PlaybackFinished&) override;
  void react(const audio::QueueUpdate&) override;
  using UiState::react;
};

class FatalError : public UiState {};

}  // namespace states

}  // namespace ui
