/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <vector>

#include "index.hpp"
#include "lvgl.h"

#include "modal.hpp"

namespace ui {
namespace modals {

class Confirm : public Modal {
 public:
  Confirm(Screen*, const std::string& title, bool has_cancel);

 private:
  lv_obj_t* container_;
};

}  // namespace modals
}  // namespace ui
