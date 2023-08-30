/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "service_locator.hpp"

#include <memory>

#include "nvs.hpp"
#include "touchwheel.hpp"

namespace system_fsm {

auto ServiceLocator::instance() -> ServiceLocator& {
  static ServiceLocator sInstance{};
  return sInstance;
}

}  // namespace system_fsm
