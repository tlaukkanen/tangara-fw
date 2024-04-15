/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input_hook_actions.hpp"

#include <cstdint>

#include "hal/lv_hal_indev.h"

#include "event_queue.hpp"
#include "ui_events.hpp"

namespace input {
namespace actions {

auto select(lv_indev_data_t* d) -> void {
  d->state = LV_INDEV_STATE_PRESSED;
}

auto scrollUp(lv_indev_data_t* d) -> void {
  d->enc_diff = -1;
}

auto scrollDown(lv_indev_data_t* d) -> void {
  d->enc_diff = 1;
}

auto scrollToTop(lv_indev_data_t* d) -> void {
  d->enc_diff = INT16_MIN;
}

auto scrollToBottom(lv_indev_data_t* d) -> void {
  d->enc_diff = INT16_MAX;
}

auto goBack(lv_indev_data_t* d) -> void {
  events::Ui().Dispatch(ui::internal::BackPressed{});
}

auto volumeUp(lv_indev_data_t* d) -> void {
  events::Audio().Dispatch(audio::StepUpVolume{});
}

auto volumeDown(lv_indev_data_t* d) -> void {
  events::Audio().Dispatch(audio::StepDownVolume{});
}

}  // namespace actions
}  // namespace input
