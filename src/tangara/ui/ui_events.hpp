/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "tinyfsm.hpp"

#include "database/database.hpp"
#include "database/index.hpp"
#include "drivers/gpios.hpp"
#include "drivers/nvs.hpp"
#include "ui/screen.hpp"

namespace ui {

struct OnStorageChange : tinyfsm::Event {
  bool is_mounted;
};

struct OnSystemError : tinyfsm::Event {};

struct OnLuaError : tinyfsm::Event {
  std::string message;
};

struct DumpLuaStack : tinyfsm::Event {};

struct Screenshot : tinyfsm::Event {
  std::string filename;
};

namespace internal {

struct InitDisplay : tinyfsm::Event {
  drivers::IGpios& gpios;
  drivers::NvsStorage& nvs;
};

struct ReindexDatabase : tinyfsm::Event {};

struct BackPressed : tinyfsm::Event {};
struct ShowNowPlaying : tinyfsm::Event {};

struct DismissAlerts : tinyfsm::Event {};

}  // namespace internal

}  // namespace ui
