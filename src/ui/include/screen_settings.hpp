/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <vector>

#include "index.hpp"
#include "lvgl.h"

#include "screen.hpp"

namespace ui {
namespace screens {

class Settings : public MenuScreen {
 public:
  Settings();
  ~Settings();
   private:
   std::shared_ptr<Screen> bluetooth_;
   std::shared_ptr<Screen> headphones_;
   std::shared_ptr<Screen> appearance_;
   std::shared_ptr<Screen> input_method_;
   std::shared_ptr<Screen> storage_;
   std::shared_ptr<Screen> firmware_update_;
   std::shared_ptr<Screen> about_;
};

class Bluetooth : public MenuScreen {
 public:
  Bluetooth();
};

class Headphones : public MenuScreen  {
 public:
  Headphones();
};

class Appearance : public MenuScreen  {
 public:
  Appearance();
};

class InputMethod : public MenuScreen  {
 public:
  InputMethod();
};

class Storage : public MenuScreen  {
 public:
  Storage();
};

class FirmwareUpdate : public MenuScreen  {
 public:
  FirmwareUpdate();
};

class About : public MenuScreen  {
 public:
  About();
};

}  // namespace screens
}  // namespace ui
