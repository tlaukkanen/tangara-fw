/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <string>

#include "lvgl.h"

namespace ui {

auto SaveScreenshot(lv_obj_t* obj, const std::string& path) -> void;

}
