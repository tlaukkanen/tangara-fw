/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include <vector>

#include "lvgl.h"

#include "screen.hpp"

namespace ui {
namespace screens {

class Onboarding : public Screen {
 public:
  Onboarding(const std::string& title, bool show_prev, bool show_next);

 private:
  lv_obj_t* window_;
  lv_obj_t* title_;
  lv_obj_t* next_button_;
  lv_obj_t* prev_button_;

 protected:
  lv_obj_t* content_;
};

namespace onboarding {

class LinkToManual : public Onboarding {
  LinkToManual();
};

class Controls : public Onboarding {
  Controls();
};

class FormatSdCard : public Onboarding {
  FormatSdCard();
};

}  // namespace onboarding

}  // namespace screens
}  // namespace ui