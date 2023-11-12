/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "esp_err.h"

namespace drivers {

esp_err_t spiffs_mount();

}  // namespace drivers
