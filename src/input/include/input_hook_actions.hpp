/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "hal/lv_hal_indev.h"

namespace input {
namespace actions {

auto select(lv_indev_data_t*) -> void;

auto scrollUp(lv_indev_data_t*) -> void;
auto scrollDown(lv_indev_data_t*) -> void;

auto scrollToTop(lv_indev_data_t*) -> void;
auto scrollToBottom(lv_indev_data_t*) -> void;

auto goBack(lv_indev_data_t*) -> void;

auto volumeUp(lv_indev_data_t*) -> void;
auto volumeDown(lv_indev_data_t*) -> void;

}  // namespace actions
}  // namespace input
