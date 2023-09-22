/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "screen_settings.hpp"
#include <stdint.h>
#include <string>

#include "audio_events.hpp"
#include "bluetooth.hpp"
#include "bluetooth_types.hpp"
#include "core/lv_event.h"
#include "core/lv_obj.h"
#include "core/lv_obj_tree.h"
#include "display.hpp"
#include "esp_log.h"

#include "core/lv_group.h"
#include "core/lv_obj_pos.h"
#include "event_queue.hpp"
#include "extra/layouts/flex/lv_flex.h"
#include "extra/widgets/list/lv_list.h"
#include "extra/widgets/menu/lv_menu.h"
#include "extra/widgets/spinbox/lv_spinbox.h"
#include "extra/widgets/spinner/lv_spinner.h"
#include "hal/lv_hal_disp.h"
#include "index.hpp"
#include "misc/lv_anim.h"
#include "misc/lv_area.h"
#include "nvs.hpp"
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
#include "wm8523.hpp"

namespace ui {
namespace screens {

using Page = internal::ShowSettingsPage::Page;

static void open_sub_menu_cb(lv_event_t* e) {
  Page next_page = static_cast<Page>(reinterpret_cast<uintptr_t>(e->user_data));
  events::Ui().Dispatch(internal::ShowSettingsPage{
      .page = next_page,
  });
}

static void sub_menu(lv_obj_t* list,
                     lv_group_t* group,
                     const std::string& text,
                     Page page) {
  lv_obj_t* item = lv_list_add_btn(list, NULL, text.c_str());
  lv_group_add_obj(group, item);
  lv_obj_add_event_cb(item, open_sub_menu_cb, LV_EVENT_CLICKED,
                      reinterpret_cast<void*>(static_cast<uintptr_t>(page)));
}

Settings::Settings() : MenuScreen("Settings") {
  lv_obj_t* list = lv_list_create(content_);
  lv_obj_set_size(list, lv_pct(100), lv_pct(100));

  lv_list_add_text(list, "Audio");
  sub_menu(list, group_, "Bluetooth", Page::kBluetooth);
  sub_menu(list, group_, "Headphones", Page::kHeadphones);

  lv_list_add_text(list, "Interface");
  sub_menu(list, group_, "Appearance", Page::kAppearance);
  sub_menu(list, group_, "Input Method", Page::kInput);

  lv_list_add_text(list, "System");
  sub_menu(list, group_, "Storage", Page::kStorage);
  sub_menu(list, group_, "Firmware Update", Page::kFirmwareUpdate);
  sub_menu(list, group_, "About", Page::kAbout);
}

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

static auto toggle_bt_cb(lv_event_t* ev) {
  Bluetooth* instance = reinterpret_cast<Bluetooth*>(ev->user_data);
  instance->ChangeEnabledState(lv_obj_has_state(ev->target, LV_STATE_CHECKED));
}

static auto select_device_cb(lv_event_t* ev) {
  Bluetooth* instance = reinterpret_cast<Bluetooth*>(ev->user_data);
  instance->OnDeviceSelected(lv_obj_get_index(ev->target));
}

Bluetooth::Bluetooth(drivers::Bluetooth& bt, drivers::NvsStorage& nvs)
    : MenuScreen("Bluetooth"), bt_(bt), nvs_(nvs) {
  lv_obj_t* toggle_container = settings_container(content_);
  lv_obj_t* toggle_label = lv_label_create(toggle_container);
  lv_label_set_text(toggle_label, "Enable");
  lv_obj_set_flex_grow(toggle_label, 1);
  lv_obj_t* toggle = lv_switch_create(toggle_container);
  lv_group_add_obj(group_, toggle);

  if (bt.IsEnabled()) {
    lv_obj_add_state(toggle, LV_STATE_CHECKED);
  }

  lv_obj_add_event_cb(toggle, toggle_bt_cb, LV_EVENT_VALUE_CHANGED, this);

  lv_obj_t* devices_label = lv_label_create(content_);
  lv_label_set_text(devices_label, "Devices");

  devices_list_ = lv_list_create(content_);
  RefreshDevicesList();
}

auto Bluetooth::ChangeEnabledState(bool enabled) -> void {
  if (enabled) {
    events::System().RunOnTask([&]() { bt_.Enable(); });
    nvs_.OutputMode(drivers::NvsStorage::Output::kBluetooth).get();
  } else {
    events::System().RunOnTask([&]() { bt_.Disable(); });
    nvs_.OutputMode(drivers::NvsStorage::Output::kHeadphones).get();
  }
  events::Audio().Dispatch(audio::OutputModeChanged{});
  RefreshDevicesList();
}

auto Bluetooth::RefreshDevicesList() -> void {
  if (!bt_.IsEnabled()) {
    // Bluetooth is disabled, so we just clear the list.
    RemoveAllDevices();
    return;
  }

  auto devices = bt_.KnownDevices();
  std::optional<drivers::bluetooth::mac_addr_t> preferred_device =
      nvs_.PreferredBluetoothDevice().get();

  // If the user's current selection is within the devices list, then we need
  // to be careful not to rearrange the list items underneath them.
  lv_obj_t* current_selection = lv_group_get_focused(group_);
  bool is_in_list = current_selection != NULL &&
                    lv_obj_get_parent(current_selection) == devices_list_;

  if (!is_in_list) {
    // The user isn't in the list! We can blow everything away and recreate it
    // without issues.
    RemoveAllDevices();

    // First look to see if the user's preferred device is in the list. If it
    // is, we hoist it up to the top of the list.
    if (preferred_device) {
      for (size_t i = 0; i < devices.size(); i++) {
        if (devices[i].address == *preferred_device) {
          AddPreferredDevice(devices[i]);
          devices.erase(devices.begin() + i);
          break;
        }
      }
    }

    // The rest of the list is already sorted by signal strength.
    for (const auto& device : devices) {
      AddDevice(device);
    }
  } else {
    // The user's selection is within the device list. We need to work out
    // which devices are new, then add them to the end.
    for (const auto& mac : macs_in_list_) {
      auto pos = std::find_if(
          devices.begin(), devices.end(),
          [&mac](const auto& device) { return device.address == mac; });

      if (pos != devices.end()) {
        devices.erase(pos);
      }
    }

    // The remaining list is now just the new devices.
    for (const auto& device : devices) {
      if (preferred_device && device.address == *preferred_device) {
        AddPreferredDevice(device);
      } else {
        AddDevice(device);
      }
    }
  }
}

auto Bluetooth::RemoveAllDevices() -> void {
  while (lv_obj_get_child_cnt(devices_list_) > 0) {
    lv_obj_del(lv_obj_get_child(devices_list_, 0));
  }
  macs_in_list_.clear();
  preferred_device_ = nullptr;
}

auto Bluetooth::AddPreferredDevice(const drivers::bluetooth::Device& dev)
    -> void {
  preferred_device_ = lv_list_add_btn(devices_list_, NULL, dev.name.c_str());
  macs_in_list_.push_back(dev.address);
}

auto Bluetooth::AddDevice(const drivers::bluetooth::Device& dev) -> void {
  lv_obj_t* item = lv_list_add_btn(devices_list_, NULL, dev.name.c_str());
  lv_group_add_obj(group_, item);
  lv_obj_add_event_cb(item, select_device_cb, LV_EVENT_CLICKED, this);
  macs_in_list_.push_back(dev.address);
}

auto Bluetooth::OnDeviceSelected(size_t index) -> void {
  // Tell the bluetooth driver that our preference changed.
  auto it = macs_in_list_.begin();
  std::advance(it, index);
  events::System().RunOnTask([=]() { bt_.SetPreferredDevice(*it); });

  // Update which devices are selectable.
  if (preferred_device_) {
    lv_group_add_obj(group_, preferred_device_);
    // Bubble the newly added object up to its visible position in the list.
    size_t pos = lv_obj_get_index(preferred_device_);
    while (pos > 0) {
      lv_group_swap_obj(preferred_device_,
                        lv_obj_get_child(devices_list_, pos - 1));
      pos--;
    }
  }

  preferred_device_ = lv_obj_get_child(devices_list_, index);
  lv_group_remove_obj(preferred_device_);
}

static void change_vol_limit_cb(lv_event_t* ev) {
  int selected_index = lv_dropdown_get_selected(ev->target);
  Headphones* instance = reinterpret_cast<Headphones*>(ev->user_data);
  instance->ChangeMaxVolume(selected_index);
}

static void increase_vol_limit_cb(lv_event_t* ev) {
  Headphones* instance = reinterpret_cast<Headphones*>(ev->user_data);
  instance->ChangeCustomVolume(2);
}

static void decrease_vol_limit_cb(lv_event_t* ev) {
  Headphones* instance = reinterpret_cast<Headphones*>(ev->user_data);
  instance->ChangeCustomVolume(-2);
}

Headphones::Headphones(drivers::NvsStorage& nvs)
    : MenuScreen("Headphones"), nvs_(nvs), custom_limit_(0) {
  uint16_t reference = drivers::wm8523::kLineLevelReferenceVolume;
  index_to_level_.push_back(reference - (10 * 4));
  index_to_level_.push_back(reference + (6 * 4));
  index_to_level_.push_back(reference + (9.5 * 4));

  lv_obj_t* vol_label = lv_label_create(content_);
  lv_label_set_text(vol_label, "Volume Limit");
  lv_obj_t* vol_dropdown = lv_dropdown_create(content_);
  lv_dropdown_set_options(vol_dropdown,
                          "Line Level (-10 dB)\nCD Level (+6 dB)\nMax "
                          "before clipping (+10dB)\nCustom");
  lv_group_add_obj(group_, vol_dropdown);

  uint16_t level = nvs.AmpMaxVolume().get();
  for (int i = 0; i < index_to_level_.size() + 1; i++) {
    if (i == index_to_level_.size() || index_to_level_[i] == level) {
      lv_dropdown_set_selected(vol_dropdown, i);
      break;
    }
  }

  lv_obj_add_event_cb(vol_dropdown, change_vol_limit_cb, LV_EVENT_VALUE_CHANGED,
                      this);

  custom_vol_container_ = settings_container(content_);

  lv_obj_t* decrease_btn = lv_btn_create(custom_vol_container_);
  lv_obj_t* btn_label = lv_label_create(decrease_btn);
  lv_label_set_text(btn_label, "-");
  lv_obj_add_event_cb(decrease_btn, decrease_vol_limit_cb, LV_EVENT_CLICKED,
                      this);

  custom_vol_label_ = lv_label_create(custom_vol_container_);
  UpdateCustomVol(level);

  lv_obj_t* increase_btn = lv_btn_create(custom_vol_container_);
  btn_label = lv_label_create(increase_btn);
  lv_label_set_text(btn_label, "+");
  lv_obj_add_event_cb(increase_btn, increase_vol_limit_cb, LV_EVENT_CLICKED,
                      this);

  if (lv_dropdown_get_selected(vol_dropdown) != index_to_level_.size()) {
    lv_obj_add_flag(custom_vol_container_, LV_OBJ_FLAG_HIDDEN);
  }

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

auto Headphones::ChangeMaxVolume(uint8_t index) -> void {
  if (index >= index_to_level_.size()) {
    lv_obj_clear_flag(custom_vol_container_, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  auto vol = index_to_level_[index];
  lv_obj_add_flag(custom_vol_container_, LV_OBJ_FLAG_HIDDEN);
  UpdateCustomVol(vol);
  events::Audio().Dispatch(audio::ChangeMaxVolume{.new_max = vol});
}

auto Headphones::ChangeCustomVolume(int8_t diff) -> void {
  UpdateCustomVol(custom_limit_ + diff);
}

auto Headphones::UpdateCustomVol(uint16_t level) -> void {
  custom_limit_ = level;
  int16_t db = (static_cast<int32_t>(level) -
                drivers::wm8523::kLineLevelReferenceVolume) /
               4;
  int16_t db_parts = (static_cast<int32_t>(level) -
                      drivers::wm8523::kLineLevelReferenceVolume) %
                     4;

  std::ostringstream builder;
  if (db >= 0) {
    builder << "+";
  }
  builder << db << ".";
  builder << (db_parts * 100 / 4);
  builder << " dBV";

  lv_label_set_text(custom_vol_label_, builder.str().c_str());
}

static void change_brightness_cb(lv_event_t* ev) {
  Appearance* instance = reinterpret_cast<Appearance*>(ev->user_data);
  instance->ChangeBrightness(lv_slider_get_value(ev->target));
}

static void release_brightness_cb(lv_event_t* ev) {
  Appearance* instance = reinterpret_cast<Appearance*>(ev->user_data);
  instance->CommitBrightness();
}

static auto brightness_str(uint_fast8_t percent) -> std::string {
  return std::to_string(percent) + "%";
}

Appearance::Appearance(drivers::NvsStorage& nvs, drivers::Display& display)
    : MenuScreen("Appearance"), nvs_(nvs), display_(display) {
  lv_obj_t* toggle_container = settings_container(content_);
  lv_obj_t* toggle_label = lv_label_create(toggle_container);
  lv_obj_set_flex_grow(toggle_label, 1);
  lv_label_set_text(toggle_label, "Dark Mode");
  lv_obj_t* toggle = lv_switch_create(toggle_container);
  lv_group_add_obj(group_, toggle);

  uint_fast8_t initial_brightness = nvs_.ScreenBrightness().get();

  lv_obj_t* brightness_label = lv_label_create(content_);
  lv_label_set_text(brightness_label, "Brightness");
  lv_obj_t* brightness = lv_slider_create(content_);
  lv_obj_set_width(brightness, lv_pct(100));
  lv_slider_set_range(brightness, 10, 100);
  lv_slider_set_value(brightness, initial_brightness, LV_ANIM_OFF);
  lv_group_add_obj(group_, brightness);
  current_brightness_label_ = lv_label_create(content_);
  lv_label_set_text(current_brightness_label_,
                    brightness_str(initial_brightness).c_str());
  lv_obj_set_size(current_brightness_label_, lv_pct(100), LV_SIZE_CONTENT);

  lv_obj_add_event_cb(brightness, change_brightness_cb, LV_EVENT_VALUE_CHANGED,
                      this);
  lv_obj_add_event_cb(brightness, release_brightness_cb, LV_EVENT_RELEASED,
                      this);
}

auto Appearance::ChangeBrightness(uint_fast8_t new_level) -> void {
  current_brightness_ = new_level;
  display_.SetBrightness(new_level);
  lv_label_set_text(current_brightness_label_,
                    brightness_str(new_level).c_str());
}

auto Appearance::CommitBrightness() -> void {
  nvs_.ScreenBrightness(current_brightness_);
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
