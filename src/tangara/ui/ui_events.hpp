/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include "database/database.hpp"
#include "database/index.hpp"
#include "drivers/gpios.hpp"
#include "drivers/nvs.hpp"
#include "tinyfsm.hpp"
#include "ui/screen.hpp"

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

struct DumpLuaStack : tinyfsm::Event {};

namespace internal {

struct InitDisplay : tinyfsm::Event {
  drivers::IGpios& gpios;
  drivers::NvsStorage& nvs;
};

struct ReindexDatabase : tinyfsm::Event {};

struct BackPressed : tinyfsm::Event {};
struct ShowNowPlaying : tinyfsm::Event {};

struct ModalConfirmPressed : tinyfsm::Event {};
struct ModalCancelPressed : tinyfsm::Event {};

struct DismissAlerts : tinyfsm::Event {};

}  // namespace internal

}  // namespace ui
