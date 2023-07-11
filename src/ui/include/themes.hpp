#pragma once

#include "lvgl.h"

namespace ui {
namespace themes {
class Theme {
 public:
  Theme();
  void Apply(void);
  void Callback(lv_obj_t* obj);

 private:
  lv_style_t button_style_;
  lv_theme_t theme_;
};
}  // namespace themes
}  // namespace ui