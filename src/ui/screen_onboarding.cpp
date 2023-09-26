/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen_onboarding.hpp"

#include "core/lv_event.h"
#include "core/lv_obj_pos.h"
#include "draw/lv_draw_rect.h"
#include "event_queue.hpp"
#include "extra/libs/qrcode/lv_qrcode.h"
#include "extra/widgets/win/lv_win.h"
#include "font/lv_symbol_def.h"
#include "misc/lv_color.h"
#include "ui_events.hpp"
#include "widgets/lv_btn.h"
#include "widgets/lv_label.h"
#include "widgets/lv_switch.h"

static const char kManualUrl[] = "https://tangara.gay/onboarding";

namespace ui {
namespace screens {

static void next_btn_cb(lv_event_t* ev) {
  events::Ui().Dispatch(internal::OnboardingNavigate{.forwards = true});
}

static void prev_btn_cb(lv_event_t* ev) {
  events::Ui().Dispatch(internal::OnboardingNavigate{.forwards = false});
}

Onboarding::Onboarding(const std::pmr::string& title,
                       bool show_prev,
                       bool show_next) {
  window_ = lv_win_create(root_, 18);
  if (show_prev) {
    prev_button_ = lv_win_add_btn(window_, LV_SYMBOL_LEFT, 20);
    lv_obj_add_event_cb(prev_button_, prev_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(group_, prev_button_);
  }
  title_ = lv_win_add_title(window_, title.c_str());
  if (show_next) {
    next_button_ = lv_win_add_btn(window_, LV_SYMBOL_RIGHT, 20);
    lv_obj_add_event_cb(next_button_, next_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(group_, next_button_);
  }

  content_ = lv_win_get_content(window_);
  lv_obj_set_layout(content_, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
}

namespace onboarding {

LinkToManual::LinkToManual() : Onboarding("Welcome!", false, true) {
  lv_obj_t* intro = lv_label_create(content_);
  lv_label_set_text(intro, "For full instructions, see the manual:");
  lv_label_set_long_mode(intro, LV_LABEL_LONG_WRAP);
  lv_obj_set_size(intro, lv_pct(100), LV_SIZE_CONTENT);

  lv_obj_t* qr =
      lv_qrcode_create(content_, 80, lv_color_black(), lv_color_white());
  lv_qrcode_update(qr, kManualUrl, sizeof(kManualUrl));
}

static void create_radio_button(lv_obj_t* parent,
                                const std::pmr::string& text) {
  lv_obj_t* obj = lv_checkbox_create(parent);
  lv_checkbox_set_text(obj, text.c_str());
  // TODO: radio styling
}

Controls::Controls() : Onboarding("Controls", true, true) {
  lv_obj_t* label = lv_label_create(content_);
  lv_label_set_text(label, "this screen changes your control scheme.");

  label = lv_label_create(content_);
  lv_label_set_text(label, "how does the touch wheel behave?");

  create_radio_button(content_, "iPod-style");
  create_radio_button(content_, "Directional");
  create_radio_button(content_, "One Big Button");

  label = lv_label_create(content_);
  lv_label_set_text(label, "how do the side buttons behave?");

  create_radio_button(content_, "Adjust volume");
  create_radio_button(content_, "Scroll");
}

MissingSdCard::MissingSdCard() : Onboarding("SD Card", true, false) {
  lv_obj_t* label = lv_label_create(content_);
  lv_label_set_text(label,
                    "It looks like there isn't an SD card present. Please "
                    "insert one to continue.");
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_size(label, lv_pct(100), LV_SIZE_CONTENT);
}

FormatSdCard::FormatSdCard() : Onboarding("SD Card", true, false) {
  lv_obj_t* label = lv_label_create(content_);
  lv_label_set_text(label,
                    "It looks like there is an SD card present, but it has not "
                    "been formatted. Would you like to format it?");
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_size(label, lv_pct(100), LV_SIZE_CONTENT);

  lv_obj_t* button = lv_btn_create(content_);
  label = lv_label_create(button);
  lv_label_set_text(label, "Format");

  lv_obj_t* exfat_con = lv_obj_create(content_);
  lv_obj_set_layout(exfat_con, LV_LAYOUT_FLEX);
  lv_obj_set_size(exfat_con, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(exfat_con, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(exfat_con, LV_FLEX_ALIGN_SPACE_EVENLY,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

  label = lv_label_create(exfat_con);
  lv_label_set_text(label, "Use exFAT");
  lv_switch_create(exfat_con);
}

InitDatabase::InitDatabase() : Onboarding("Database", true, true) {
  lv_obj_t* label = lv_label_create(content_);
  lv_label_set_text(label,
                    "Many of Tangara's browsing features rely building an "
                    "index of your music. Would you like to do this now? It "
                    "will take some time if you have a large collection.");
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_size(label, lv_pct(100), LV_SIZE_CONTENT);

  lv_obj_t* button = lv_btn_create(content_);
  label = lv_label_create(button);
  lv_label_set_text(label, "Index");
}

}  // namespace onboarding

}  // namespace screens
}  // namespace ui
