/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <mutex>
#include "esp_err.h"

namespace drivers {

esp_err_t init_spi(void);
esp_err_t deinit_spi(void);

}  // namespace drivers
