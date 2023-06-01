/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "esp_timer.h"

#define LV_TICK_CUSTOM_SYS_TIME_EXPR (esp_timer_get_time() / 1000)
