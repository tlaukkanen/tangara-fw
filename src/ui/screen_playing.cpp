/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen_playing.hpp"

#include "esp_log.h"
#include "lvgl.h"

#include "core/lv_group.h"
#include "core/lv_obj_pos.h"
#include "event_queue.hpp"
#include "extra/widgets/list/lv_list.h"
#include "extra/widgets/menu/lv_menu.h"
#include "extra/widgets/spinner/lv_spinner.h"
#include "hal/lv_hal_disp.h"
#include "index.hpp"
#include "misc/lv_area.h"
#include "track.hpp"
#include "ui_events.hpp"
#include "ui_fsm.hpp"
#include "widgets/lv_label.h"

namespace ui {
namespace screens {

Playing::Playing(database::Track track) : track_(track) {
  lv_obj_t* container = lv_obj_create(root_);
  lv_obj_set_align(container, LV_ALIGN_CENTER);
  lv_obj_set_size(container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

  // bro idk
  lv_obj_t* label = lv_label_create(container);
  lv_label_set_text_static(label, track.TitleOrFilename().c_str());
  lv_obj_set_align(label, LV_ALIGN_CENTER);
}

Playing::~Playing() {}

}  // namespace screens
}  // namespace ui
