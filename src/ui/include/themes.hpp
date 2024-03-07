#pragma once

#include <string>
#include <map>
#include <vector>
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

  void AddStyle(std::string key, int selector, lv_style_t* style);

  static auto instance() -> Theme*;

 private:
  Theme();
  std::map<std::string, std::vector<std::pair<int, lv_style_t*>>> style_map;
  lv_theme_t theme_;

};
}  // namespace themes
}  // namespace ui
