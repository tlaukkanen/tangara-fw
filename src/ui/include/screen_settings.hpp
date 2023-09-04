/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstdint>
#include <memory>
#include <vector>

#include "display.hpp"
#include "index.hpp"
#include "lvgl.h"

#include "nvs.hpp"
#include "screen.hpp"

namespace ui {
namespace screens {

class Settings : public MenuScreen {
 public:
  Settings();
};

class Bluetooth : public MenuScreen {
 public:
  Bluetooth();
};

class Headphones : public MenuScreen {
 public:
  Headphones(drivers::NvsStorage& nvs);

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
  Appearance(drivers::NvsStorage& nvs, drivers::Display& display);

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
  InputMethod();
};

class Storage : public MenuScreen {
 public:
  Storage();
};

class FirmwareUpdate : public MenuScreen {
 public:
  FirmwareUpdate();
};

class About : public MenuScreen {
 public:
  About();
};

}  // namespace screens
}  // namespace ui
