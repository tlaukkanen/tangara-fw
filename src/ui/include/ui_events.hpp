/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "database.hpp"
#include "index.hpp"
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

namespace internal {

struct RecordSelected : tinyfsm::Event {
  database::IndexRecord record;
};

struct IndexSelected : tinyfsm::Event {
  database::IndexInfo index;
};

}  // namespace internal

}  // namespace ui
