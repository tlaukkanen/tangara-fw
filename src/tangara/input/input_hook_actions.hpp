/*
 * Copyright 2024 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "input/input_hook.hpp"

namespace input {
namespace actions {

auto select() -> HookCallback;

auto scrollUp() -> HookCallback;
auto scrollDown() -> HookCallback;

auto scrollToTop() -> HookCallback;
auto scrollToBottom() -> HookCallback;

auto goBack() -> HookCallback;

auto volumeUp() -> HookCallback;
auto volumeDown() -> HookCallback;

auto allActions() -> std::vector<HookCallback>;

}  // namespace actions
}  // namespace input
