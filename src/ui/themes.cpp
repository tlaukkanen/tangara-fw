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

  // TODO: Apply widget style

}

void Theme::ApplyStyle(lv_obj_t* obj, Style style) {}

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
