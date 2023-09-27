/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>

#include "lvgl.h"

#include "core/lv_event.h"
#include "core/lv_obj.h"
#include "nod/nod.hpp"

namespace ui {

class EventBinding {
 public:
  EventBinding(lv_obj_t* obj, lv_event_code_t ev);

  auto signal() -> nod::signal<void(lv_obj_t*)>& { return signal_; }

 private:
  lv_obj_t* obj_;
  nod::signal<void(lv_obj_t*)> signal_;
};

}  // namespace ui
