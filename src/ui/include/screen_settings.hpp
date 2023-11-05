/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>
#include <list>
#include <memory>
#include <vector>

#include "bluetooth.hpp"
#include "bluetooth_types.hpp"
#include "display.hpp"
#include "index.hpp"
#include "lvgl.h"

#include "model_top_bar.hpp"
#include "nvs.hpp"
#include "screen.hpp"

namespace ui {
namespace screens {

class Settings : public MenuScreen {
 public:
  Settings(models::TopBar&);
};

class Bluetooth : public MenuScreen {
 public:
  Bluetooth(models::TopBar&, drivers::Bluetooth& bt, drivers::NvsStorage& nvs);
  ~Bluetooth();

  auto ChangeEnabledState(bool enabled) -> void;
  auto RefreshDevicesList() -> void;
  auto OnDeviceSelected(ssize_t index) -> void;

 private:
  auto RemoveAllDevices() -> void;
  auto AddPreferredDevice(const drivers::bluetooth::Device&) -> void;
  auto AddDevice(const drivers::bluetooth::Device&) -> void;

  drivers::Bluetooth& bt_;
  drivers::NvsStorage& nvs_;

  lv_obj_t* devices_list_;
  lv_obj_t* preferred_device_;

  std::list<drivers::bluetooth::mac_addr_t> macs_in_list_;
};

class Headphones : public MenuScreen {
 public:
  Headphones(models::TopBar&, drivers::NvsStorage& nvs);

  auto ChangeMaxVolume(uint8_t index) -> void;
  auto ChangeCustomVolume(int8_t diff) -> void;

 private:
  auto UpdateCustomVol(uint16_t) -> void;

  drivers::NvsStorage& nvs_;
  lv_obj_t* custom_vol_container_;
  lv_obj_t* custom_vol_label_;

  std::vector<uint16_t> index_to_level_;
  uint16_t custom_limit_;
};

class Appearance : public MenuScreen {
 public:
  Appearance(models::TopBar&,
             drivers::NvsStorage& nvs,
             drivers::Display& display);

  auto ChangeBrightness(uint_fast8_t) -> void;
  auto CommitBrightness() -> void;

 private:
  drivers::NvsStorage& nvs_;
  drivers::Display& display_;

  lv_obj_t* current_brightness_label_;
  uint_fast8_t current_brightness_;
};

class InputMethod : public MenuScreen {
 public:
  InputMethod(models::TopBar&, drivers::NvsStorage& nvs);

 private:
  drivers::NvsStorage& nvs_;
};

class Storage : public MenuScreen {
 public:
  Storage(models::TopBar&);
};

class FirmwareUpdate : public MenuScreen {
 public:
  FirmwareUpdate(models::TopBar&);
};

class About : public MenuScreen {
 public:
  About(models::TopBar&);
};

}  // namespace screens
}  // namespace ui
