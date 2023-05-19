#pragma once

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

}  // namespace ui
