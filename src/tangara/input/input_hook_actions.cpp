/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "input/input_hook_actions.hpp"

#include <cstdint>

#include "indev/lv_indev.h"

#include "events/event_queue.hpp"
#include "ui/ui_events.hpp"

namespace input {
namespace actions {

auto select() -> HookCallback {
  return HookCallback{.name = "select", .fn = [&](lv_indev_data_t* d) {
                        d->state = LV_INDEV_STATE_PRESSED;
                      }};
}

auto scrollUp() -> HookCallback {
  return HookCallback{.name = "scroll_up",
                      .fn = [&](lv_indev_data_t* d) { d->enc_diff = -1; }};
}

auto scrollDown() -> HookCallback {
  return HookCallback{.name = "scroll_down",
                      .fn = [&](lv_indev_data_t* d) { d->enc_diff = 1; }};
}

auto scrollToTop() -> HookCallback {
  return HookCallback{.name = "scroll_to_top", .fn = [&](lv_indev_data_t* d) {
                        d->enc_diff = INT16_MIN;
                      }};
}

auto scrollToBottom() -> HookCallback {
  return HookCallback{
      .name = "scroll_to_bottom",
      .fn = [&](lv_indev_data_t* d) { d->enc_diff = INT16_MAX; }};
}

auto goBack() -> HookCallback {
  return HookCallback{.name = "back", .fn = [&](lv_indev_data_t* d) {
                        events::Ui().Dispatch(ui::internal::BackPressed{});
                      }};
}

auto volumeUp() -> HookCallback {
  return HookCallback{.name = "volume_up", .fn = [&](lv_indev_data_t* d) {
                        events::Audio().Dispatch(audio::StepUpVolume{});
                      }};
}

auto volumeDown() -> HookCallback {
  return HookCallback{.name = "volume_down", .fn = [&](lv_indev_data_t* d) {
                        events::Audio().Dispatch(audio::StepDownVolume{});
                      }};
}

auto allActions() -> std::vector<HookCallback> {
  return {
      select(),         scrollUp(), scrollDown(), scrollToTop(),
      scrollToBottom(), goBack(),   volumeUp(),   volumeDown(),
  };
}

}  // namespace actions
}  // namespace input
