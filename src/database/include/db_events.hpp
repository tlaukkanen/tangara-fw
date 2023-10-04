/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include "tinyfsm.hpp"

namespace database {
namespace event {

struct UpdateStarted : tinyfsm::Event {};

struct UpdateFinished : tinyfsm::Event {};

struct UpdateProgress : tinyfsm::Event {
  enum class Stage {
    kVerifyingExistingTracks,
    kScanningForNewTracks,
  };
  Stage stage;
  uint64_t val;
};

}  // namespace event
}  // namespace database
