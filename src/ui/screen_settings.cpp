/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen_settings.hpp"

#include "core/lv_event.h"
#include "core/lv_obj.h"
#include "esp_log.h"

#include "core/lv_group.h"
#include "core/lv_obj_pos.h"
#include "event_queue.hpp"
#include "extra/layouts/flex/lv_flex.h"
#include "extra/widgets/list/lv_list.h"
#include "extra/widgets/menu/lv_menu.h"
#include "extra/widgets/spinner/lv_spinner.h"
#include "hal/lv_hal_disp.h"
#include "index.hpp"
#include "misc/lv_anim.h"
#include "misc/lv_area.h"
#include "screen.hpp"
#include "ui_events.hpp"
#include "ui_fsm.hpp"
#include "widget_top_bar.hpp"
#include "widgets/lv_bar.h"
#include "widgets/lv_btn.h"
#include "widgets/lv_dropdown.h"
#include "widgets/lv_label.h"
#include "widgets/lv_slider.h"
#include "widgets/lv_switch.h"

namespace ui {
namespace screens {

static void open_sub_menu_cb(lv_event_t* e) {
  std::shared_ptr<Screen>* next_screen =
      reinterpret_cast<std::shared_ptr<Screen>*>(e->user_data);
  events::Ui().Dispatch(internal::ShowSettingsPage{
      .screen = *next_screen,
  });
}

static void sub_menu(lv_obj_t* list,
                     lv_group_t* group,
                     const std::string& text,
                     std::shared_ptr<Screen>* screen) {
  lv_obj_t* item = lv_list_add_btn(list, NULL, text.c_str());
  lv_group_add_obj(group, item);
  lv_obj_add_event_cb(item, open_sub_menu_cb, LV_EVENT_CLICKED, screen);
}

Settings::Settings()
    : MenuScreen("Settings"),
      bluetooth_(new Bluetooth()),
      headphones_(new Headphones()),
      appearance_(new Appearance()),
      input_method_(new InputMethod()),
      storage_(new Storage()),
      firmware_update_(new FirmwareUpdate()),
      about_(new About()) {
  lv_obj_t* list = lv_list_create(content_);
  lv_obj_set_size(list, lv_pct(100), lv_pct(100));

  lv_list_add_text(list, "Audio");
  sub_menu(list, group_, "Bluetooth", &bluetooth_);
  sub_menu(list, group_, "Headphones", &headphones_);

  lv_list_add_text(list, "Interface");
  sub_menu(list, group_, "Appearance", &appearance_);
  sub_menu(list, group_, "Input Method", &input_method_);

  lv_list_add_text(list, "System");
  sub_menu(list, group_, "Storage", &storage_);
  sub_menu(list, group_, "Firmware Update", &firmware_update_);
  sub_menu(list, group_, "About", &about_);
}

Settings::~Settings() {}

static auto settings_container(lv_obj_t* parent) -> lv_obj_t* {
  lv_obj_t* res = lv_obj_create(parent);
  lv_obj_set_layout(res, LV_LAYOUT_FLEX);
  lv_obj_set_size(res, lv_pct(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(res, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(res, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_START);
  return res;
}

static auto label_pair(lv_obj_t* parent,
                       const std::string& left,
                       const std::string& right) -> lv_obj_t* {
  lv_obj_t* container = settings_container(parent);
  lv_obj_t* left_label = lv_label_create(container);
  lv_label_set_text(left_label, left.c_str());
  lv_obj_t* right_label = lv_label_create(container);
  lv_label_set_text(right_label, right.c_str());
  return right_label;
}

Bluetooth::Bluetooth() : MenuScreen("Bluetooth") {
  lv_obj_t* toggle_container = settings_container(content_);
  lv_obj_t* toggle_label = lv_label_create(toggle_container);
  lv_label_set_text(toggle_label, "Enable");
  lv_obj_set_flex_grow(toggle_label, 1);
  lv_obj_t* toggle = lv_switch_create(toggle_container);
  lv_group_add_obj(group_, toggle);

  lv_obj_t* devices_label = lv_label_create(content_);
  lv_label_set_text(devices_label, "Devices");

  lv_obj_t* devices_list = lv_list_create(content_);
  lv_list_add_text(devices_list, "My Headphones");
  lv_group_add_obj(group_,
                   lv_list_add_btn(devices_list, NULL, "Something else"));
  lv_group_add_obj(group_, lv_list_add_btn(devices_list, NULL, "A car??"));
  lv_group_add_obj(group_,
                   lv_list_add_btn(devices_list, NULL, "OLED TV ANDROID"));
  lv_group_add_obj(
      group_, lv_list_add_btn(devices_list, NULL, "there could be another"));
  lv_group_add_obj(group_, lv_list_add_btn(devices_list, NULL,
                                           "this one has a really long name"));
}

Headphones::Headphones() : MenuScreen("Headphones") {
  lv_obj_t* vol_label = lv_label_create(content_);
  lv_label_set_text(vol_label, "Volume Limit");
  lv_obj_t* vol_dropdown = lv_dropdown_create(content_);
  lv_dropdown_set_options(vol_dropdown,
                          "Line Level (-10 dBV)\nPro Level (+4 dBu)\nMax "
                          "before clipping\nUnlimited\nCustom");
  lv_group_add_obj(group_, vol_dropdown);

  lv_obj_t* warning_label = label_pair(
      content_, "!!", "Changing volume limit is for advanced users.");
  lv_label_set_long_mode(warning_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_flex_grow(warning_label, 1);

  lv_obj_t* balance_label = lv_label_create(content_);
  lv_label_set_text(balance_label, "Left/Right Balance");
  lv_obj_t* balance = lv_slider_create(content_);
  lv_obj_set_width(balance, lv_pct(100));
  lv_slider_set_range(balance, -10, 10);
  lv_slider_set_value(balance, 0, LV_ANIM_OFF);
  lv_slider_set_mode(balance, LV_SLIDER_MODE_SYMMETRICAL);
  lv_group_add_obj(group_, balance);
  lv_obj_t* current_balance_label = lv_label_create(content_);
  lv_label_set_text(current_balance_label, "0dB");
  lv_obj_set_size(current_balance_label, lv_pct(100), LV_SIZE_CONTENT);
}

Appearance::Appearance() : MenuScreen("Appearance") {
  lv_obj_t* toggle_container = settings_container(content_);
  lv_obj_t* toggle_label = lv_label_create(toggle_container);
  lv_obj_set_flex_grow(toggle_label, 1);
  lv_label_set_text(toggle_label, "Dark Mode");
  lv_obj_t* toggle = lv_switch_create(toggle_container);
  lv_group_add_obj(group_, toggle);
}

InputMethod::InputMethod() : MenuScreen("Input Method") {
  lv_obj_t* wheel_label = lv_label_create(content_);
  lv_label_set_text(wheel_label, "What does the wheel do?");
  lv_obj_t* wheel_dropdown = lv_dropdown_create(content_);
  lv_dropdown_set_options(wheel_dropdown, "Scroll\nDirectional\nBig Button");
  lv_group_add_obj(group_, wheel_dropdown);

  lv_obj_t* buttons_label = lv_label_create(content_);
  lv_label_set_text(buttons_label, "What do the buttons do?");
  lv_obj_t* buttons_dropdown = lv_dropdown_create(content_);
  lv_dropdown_set_options(buttons_dropdown, "Volume\nScroll");
  lv_group_add_obj(group_, buttons_dropdown);
}

Storage::Storage() : MenuScreen("Storage") {
  label_pair(content_, "Storage Capacity:", "32 GiB");
  label_pair(content_, "Currently Used:", "6 GiB");
  label_pair(content_, "DB Size:", "1.2 MiB");

  lv_obj_t* usage_bar = lv_bar_create(content_);
  lv_bar_set_range(usage_bar, 0, 32);
  lv_bar_set_value(usage_bar, 6, LV_ANIM_OFF);

  lv_obj_t* reset_btn = lv_btn_create(content_);
  lv_obj_t* reset_label = lv_label_create(reset_btn);
  lv_label_set_text(reset_label, "Reset Database");
  lv_group_add_obj(group_, reset_btn);
}

FirmwareUpdate::FirmwareUpdate() : MenuScreen("Firmware Update") {
  label_pair(content_, "ESP32 FW:", "vIDKLOL");
  label_pair(content_, "SAMD21 FW:", "vIDKLOL");

  lv_obj_t* flash_esp_btn = lv_btn_create(content_);
  lv_obj_t* flash_esp_label = lv_label_create(flash_esp_btn);
  lv_label_set_text(flash_esp_label, "Update ESP32");
  lv_group_add_obj(group_, flash_esp_btn);

  lv_obj_t* flash_samd_btn = lv_btn_create(content_);
  lv_obj_t* flash_samd_label = lv_label_create(flash_samd_btn);
  lv_label_set_text(flash_samd_label, "Update SAMD21");
  lv_group_add_obj(group_, flash_samd_btn);
}

About::About() : MenuScreen("About") {
  lv_obj_t* label = lv_label_create(content_);
  lv_label_set_text(label, "Some licenses or whatever");
}

}  // namespace screens
}  // namespace ui
