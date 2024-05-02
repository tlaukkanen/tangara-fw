
/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "modal.hpp"

#include "misc/lv_color.h"

#include "core/lv_event.h"
#include "esp_log.h"

#include "core/lv_group.h"
#include "core/lv_obj_pos.h"
#include "event_queue.hpp"
#include "extra/widgets/list/lv_list.h"
#include "extra/widgets/menu/lv_menu.h"
#include "extra/widgets/spinner/lv_spinner.h"
#include "hal/lv_hal_disp.h"
#include "index.hpp"
#include "misc/lv_area.h"
#include "screen.hpp"
#include "themes.hpp"
#include "ui_events.hpp"
#include "ui_fsm.hpp"
#include "widgets/lv_label.h"

namespace ui {

Modal::Modal(Screen* host)
    : root_(lv_obj_create(host->modal_content())),
      group_(lv_group_create()),
      host_(host) {
  lv_obj_set_style_bg_opa(host->modal_content(), LV_OPA_40, 0);

  lv_obj_set_size(root_, 120, LV_SIZE_CONTENT);
  lv_obj_center(root_);

  lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(root_, lv_color_white(), 0);

  host_->modal_group(group_);
}

Modal::~Modal() {
  host_->modal_group(nullptr);
  lv_obj_set_style_bg_opa(host_->modal_content(), LV_OPA_TRANSP, 0);

  // The group *must* be deleted first. Otherwise, focus events will be
  // generated whilst deleting the object tree, which causes a big mess.
  lv_group_del(group_);
  lv_obj_del(root_);
}

}  // namespace ui
