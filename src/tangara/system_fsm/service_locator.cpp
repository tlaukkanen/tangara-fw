/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "system_fsm/service_locator.hpp"

#include <memory>

#include "nvs.hpp"
#include "storage.hpp"
#include "touchwheel.hpp"

namespace system_fsm {

ServiceLocator::ServiceLocator() : sd_(drivers::SdState::kNotPresent) {}

}  // namespace system_fsm
