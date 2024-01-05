#pragma once

#include "lvgl.h"

namespace ui {
namespace themes {

enum class Style {
  kMenuItem,
  kMenuSubheadFirst,
  kMenuSubhead,
  kTopBar,
  kPopup,
  kTab,
  kButtonPrimary,
};

class Theme {
 public:
  void Apply(void);
  void Callback(lv_obj_t* obj);
  void ApplyStyle(lv_obj_t* obj, Style style);

  static auto instance() -> Theme*;

 private:
  Theme();

  lv_style_t base_style_;
  lv_style_t base_focused_style_;

  lv_style_t button_style_;
  lv_style_t bar_style_;
  lv_style_t dropdown_style_;

  lv_style_t slider_indicator_style_;
  lv_style_t slider_knob_style_;
  lv_style_t slider_knob_focused_style_;

  lv_style_t switch_style_;
  lv_style_t switch_indicator_style_;
  lv_style_t switch_indicator_checked_style_;
  lv_style_t switch_knob_style_;

  lv_theme_t theme_;
};
}  // namespace themes
}  // namespace ui
