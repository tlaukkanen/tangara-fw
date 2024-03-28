#include "themes.hpp"
#include "core/lv_obj.h"
#include "core/lv_obj_style.h"
#include "core/lv_obj_tree.h"
#include "draw/lv_draw_rect.h"
#include "esp_log.h"
#include "misc/lv_color.h"
#include "misc/lv_style.h"
#include "widgets/lv_bar.h"
#include "widgets/lv_slider.h"

LV_FONT_DECLARE(font_fusion_12);

namespace ui {
namespace themes {

static void theme_apply_cb(lv_theme_t* th, lv_obj_t* obj) {
  reinterpret_cast<Theme*>(th->user_data)->Callback(obj);
}

Theme::Theme() {
  lv_theme_t* parent_theme = lv_disp_get_theme(NULL);
  theme_ = *parent_theme;
  theme_.user_data = this;

  /*Set the parent theme and the style apply callback for the new theme*/
  lv_theme_set_parent(&theme_, parent_theme);
  lv_theme_set_apply_cb(&theme_, theme_apply_cb);
}

void Theme::Apply(void) {
  lv_disp_set_theme(NULL, &theme_);
}

void Theme::Callback(lv_obj_t* obj) {
  // Find and apply base styles
  if (auto search = style_map.find("base"); search != style_map.end()) {
      for (const auto& pair : search->second) {
        lv_obj_add_style(obj, pair.second, pair.first);
      }
  }

  // Determine class name
  std::string class_name;
  if (lv_obj_check_type(obj, &lv_btn_class)) {
    class_name = "button";
  } else if (lv_obj_check_type(obj, &lv_list_btn_class)) {
    class_name = "listbutton";
  } else if (lv_obj_check_type(obj, &lv_bar_class)) {
    class_name = "bar";
  } else if (lv_obj_check_type(obj, &lv_slider_class)) {
    class_name = "slider";
  } else if (lv_obj_check_type(obj, &lv_switch_class)) {
    class_name = "switch";
  } else if (lv_obj_check_type(obj, &lv_dropdown_class)) {
    class_name = "dropdown";
  } else if (lv_obj_check_type(obj, &lv_dropdownlist_class)) {
    class_name = "dropdownlist";
  }

  // Apply all styles from class
  if (auto search = style_map.find(class_name); search != style_map.end()) {
      for (const auto& pair : search->second) {
        lv_obj_add_style(obj, pair.second, pair.first);
      }
  }

}

void Theme::ApplyStyle(lv_obj_t* obj, std::string style_key) {
  if (auto search = style_map.find(style_key); search != style_map.end()) {
      for (const auto& pair : search->second) {
        lv_obj_remove_style(obj, pair.second, pair.first);
        lv_obj_add_style(obj, pair.second, pair.first);
      }
  }
}

auto Theme::instance() -> Theme* {
  static Theme sTheme{};
  return &sTheme;
}

void Theme::AddStyle(std::string key, int selector, lv_style_t* style) {
  style_map.try_emplace(key, std::vector<std::pair<int, lv_style_t*>>{});
  if (auto search = style_map.find(key); search != style_map.end()) {
    // Key exists
    auto &vec = search->second;
    // Add it to the list
    vec.push_back(std::make_pair(selector, style));
  }
}

}  // namespace themes
}  // namespace ui
