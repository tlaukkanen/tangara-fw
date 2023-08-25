/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <stack>

#include "audio_events.hpp"
#include "relative_wheel.hpp"
#include "screen_playing.hpp"
#include "tinyfsm.hpp"

#include "display.hpp"
#include "modal.hpp"
#include "screen.hpp"
#include "storage.hpp"
#include "system_events.hpp"
#include "touchwheel.hpp"
#include "track_queue.hpp"
#include "ui_events.hpp"

namespace ui {

class UiState : public tinyfsm::Fsm<UiState> {
 public:
  static auto Init(drivers::IGpios*, audio::TrackQueue*) -> bool;

  virtual ~UiState() {}

  static auto current_screen() -> std::shared_ptr<Screen> {
    return sCurrentScreen;
  }

  virtual void entry() {}
  virtual void exit() {}

  /* Fallback event handler. Does nothing. */
  void react(const tinyfsm::Event& ev) {}

  void react(const system_fsm::BatteryPercentChanged&);

  virtual void react(const audio::PlaybackStarted&) {}
  virtual void react(const audio::PlaybackUpdate&) {}
  virtual void react(const audio::QueueUpdate&) {}

  virtual void react(const system_fsm::KeyLockChanged&);

  virtual void react(const internal::RecordSelected&) {}
  virtual void react(const internal::IndexSelected&) {}
  virtual void react(const internal::BackPressed&) {}
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

  static drivers::IGpios* sIGpios;
  static audio::TrackQueue* sQueue;

  static std::shared_ptr<drivers::TouchWheel> sTouchWheel;
  static std::shared_ptr<drivers::RelativeWheel> sRelativeWheel;
  static std::shared_ptr<drivers::Display> sDisplay;
  static std::weak_ptr<database::Database> sDb;

  static std::stack<std::shared_ptr<Screen>> sScreens;
  static std::shared_ptr<Screen> sCurrentScreen;
  static std::shared_ptr<Modal> sCurrentModal;
};

namespace states {

class Splash : public UiState {
 public:
  void exit() override;
  void react(const system_fsm::BootComplete&) override;
  using UiState::react;
};

class Onboarding : public UiState {
 public:
  void entry() override;

  using UiState::react;
};

class Browse : public UiState {
  void entry() override;

  void react(const internal::RecordSelected&) override;
  void react(const internal::IndexSelected&) override;
  void react(const internal::BackPressed&) override;

  void react(const system_fsm::StorageMounted&) override;
  using UiState::react;
};

class Playing : public UiState {
  void entry() override;
  void exit() override;

  void react(const internal::BackPressed&) override;

  void react(const audio::PlaybackStarted&) override;
  void react(const audio::PlaybackUpdate&) override;
  void react(const audio::QueueUpdate&) override;
  using UiState::react;
};

class FatalError : public UiState {};

}  // namespace states

}  // namespace ui
