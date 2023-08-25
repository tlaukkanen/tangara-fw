/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen_menu.hpp"

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
#include "ui_events.hpp"
#include "ui_fsm.hpp"
#include "widget_top_bar.hpp"
#include "widgets/lv_label.h"

namespace ui {
namespace screens {

static void now_playing_click_cb(lv_event_t* ev) {
  events::Ui().Dispatch(internal::ShowNowPlaying{});
}

static void settings_click_callback(lv_event_t* ev) {
  std::shared_ptr<Screen> settings{new Settings()};
  events::Ui().Dispatch(internal::ShowSettingsPage{.screen = settings});
}

static void index_click_cb(lv_event_t* ev) {
  if (ev->user_data == NULL) {
    return;
  }
  database::IndexInfo* index =
      reinterpret_cast<database::IndexInfo*>(ev->user_data);

  events::Ui().Dispatch(internal::IndexSelected{.index = *index});
}

Menu::Menu(std::vector<database::IndexInfo> indexes)
    : MenuScreen(" ", false), indexes_(indexes) {
  lv_obj_t* list = lv_list_create(content_);
  lv_obj_set_size(list, lv_pct(100), lv_pct(100));

  lv_obj_t* now_playing = lv_list_add_btn(list, NULL, "Now Playing");
  lv_obj_add_event_cb(now_playing, now_playing_click_cb, LV_EVENT_CLICKED,
                      NULL);
  lv_group_add_obj(group_, now_playing);

  for (database::IndexInfo& index : indexes_) {
    lv_obj_t* item = lv_list_add_btn(list, NULL, index.name.c_str());
    lv_obj_add_event_cb(item, index_click_cb, LV_EVENT_CLICKED, &index);
    lv_group_add_obj(group_, item);
  }

  lv_obj_t* settings = lv_list_add_btn(list, NULL, "Settings");
  lv_obj_add_event_cb(settings, settings_click_callback, LV_EVENT_CLICKED,
                      NULL);
  lv_group_add_obj(group_, settings);
}

Menu::~Menu() {}

}  // namespace screens
}  // namespace ui
