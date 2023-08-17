/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen_onboarding.hpp"

#include "draw/lv_draw_rect.h"
#include "extra/libs/qrcode/lv_qrcode.h"
#include "extra/widgets/win/lv_win.h"
#include "font/lv_symbol_def.h"
#include "misc/lv_color.h"
#include "widgets/lv_btn.h"
#include "widgets/lv_label.h"
#include "widgets/lv_switch.h"

static const char kManualUrl[] = "https://tangara.gay/onboarding";

namespace ui {
namespace screens {

Onboarding::Onboarding(const std::string& title,
                       bool show_prev,
                       bool show_next) {
  window_ = lv_win_create(root_, 18);
  if (show_prev) {
    prev_button_ = lv_win_add_btn(window_, LV_SYMBOL_LEFT, 20);
  }
  title_ = lv_win_add_title(window_, title.c_str());
  if (show_next) {
    next_button_ = lv_win_add_btn(window_, LV_SYMBOL_RIGHT, 20);
  }

  content_ = lv_win_get_content(window_);
}

namespace onboarding {

LinkToManual::LinkToManual() : Onboarding("Welcome!", false, true) {
  lv_obj_t* intro = lv_label_create(content_);
  lv_label_set_text(intro, "this screen links you to better instructions");

  lv_obj_t* qr =
      lv_qrcode_create(content_, 100, lv_color_black(), lv_color_white());
  lv_qrcode_update(qr, kManualUrl, sizeof(kManualUrl));
}

static void create_radio_button(lv_obj_t* parent, const std::string& text) {
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

FormatSdCard::FormatSdCard() : Onboarding("SD Card", true, false) {
  lv_obj_t* label = lv_label_create(content_);
  lv_label_set_text(
      label, "this screen is optional. it offers to format your sd card.");

  lv_obj_t* button = lv_btn_create(content_);
  label = lv_label_create(button);
  lv_label_set_text(label, "Format");

  label = lv_label_create(content_);
  lv_label_set_text(label, "Advanced");

  lv_obj_t* advanced_container = lv_obj_create(content_);

  label = lv_label_create(advanced_container);
  lv_label_set_text(label, "Use exFAT");
  lv_switch_create(advanced_container);
}

}  // namespace onboarding

}  // namespace screens
}  // namespace ui
