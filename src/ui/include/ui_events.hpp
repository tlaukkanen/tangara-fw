/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include "database.hpp"
#include "index.hpp"
#include "screen.hpp"
#include "tinyfsm.hpp"

namespace ui {

// TODO(jacqueline): is this needed? is this good?
/*
 * Event emitted by the main task on heartbeat.
 */
struct OnStorageChange : tinyfsm::Event {
  bool is_mounted;
};

struct OnSystemError : tinyfsm::Event {};

  struct OnLuaError : tinyfsm::Event {
    std::string message;
  };

namespace internal {

struct ControlSchemeChanged : tinyfsm::Event {};
struct ReindexDatabase : tinyfsm::Event {};

struct BackPressed : tinyfsm::Event {};
struct ShowNowPlaying : tinyfsm::Event {};
struct ShowSettingsPage : tinyfsm::Event {
  enum class Page {
    kRoot,
    kBluetooth,
    kHeadphones,
    kAppearance,
    kInput,
    kStorage,
    kFirmwareUpdate,
    kAbout,
  } page;
};
struct OnboardingNavigate : tinyfsm::Event {
  bool forwards;
};

struct ModalConfirmPressed : tinyfsm::Event {};
struct ModalCancelPressed : tinyfsm::Event {};

}  // namespace internal

}  // namespace ui
