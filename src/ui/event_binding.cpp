/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "event_binding.hpp"

#include "core/lv_event.h"

namespace ui {

static auto event_cb(lv_event_t* ev) -> void {
  EventBinding* binding =
      static_cast<EventBinding*>(lv_event_get_user_data(ev));
  binding->signal()(lv_event_get_target(ev));
}

EventBinding::EventBinding(lv_obj_t* obj, lv_event_code_t ev) {
  lv_obj_add_event_cb(obj, event_cb, ev, this);
}

}  // namespace ui
