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

class Progress : public Modal {
 public:
  Progress(Screen*, std::string title);

 private:
  lv_obj_t* container_;
};

}  // namespace modals
}  // namespace ui
