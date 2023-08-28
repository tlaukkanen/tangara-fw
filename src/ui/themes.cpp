#include "themes.hpp"
#include "core/lv_obj.h"
#include "misc/lv_color.h"
#include "esp_log.h"

LV_FONT_DECLARE(font_fusion);

namespace ui {
namespace themes {

static void theme_apply_cb(lv_theme_t* th, lv_obj_t* obj) {
  reinterpret_cast<Theme*>(th->user_data)->Callback(obj);
}

Theme::Theme() {
  /*Initialize the styles*/
  lv_style_init(&button_style_);
  lv_style_set_pad_all(&button_style_, 2);
  lv_style_set_bg_color(&button_style_, lv_color_white());

  lv_style_init(&button_style_focused_);
  lv_style_set_bg_color(&button_style_focused_, lv_palette_lighten(LV_PALETTE_BLUE_GREY, 2));

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
  ESP_LOGI("Theme", "Callback called on object %p", obj);
  lv_obj_set_style_text_font(obj, &font_fusion, 0);
  lv_obj_set_style_text_color(obj, lv_color_black(), 0);

  if (lv_obj_check_type(obj, &lv_btn_class) ||
      lv_obj_check_type(obj, &lv_list_btn_class)) {
    lv_obj_add_style(obj, &button_style_, LV_PART_MAIN);
    lv_obj_add_style(obj, &button_style_focused_, LV_PART_MAIN | LV_STATE_FOCUSED);
  }
}

void Theme::ApplyStyle(lv_obj_t* obj, Style style) {
  ESP_LOGI("Theme", "Apply style called on object %p", obj);
  if (style == Style::kTopBar) {
    lv_obj_set_style_border_color(obj, lv_palette_darken(LV_PALETTE_BLUE_GREY, 2), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_BOTTOM|LV_BORDER_SIDE_TOP, LV_PART_MAIN);
    lv_obj_set_style_pad_top(obj, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(obj, 2, LV_PART_MAIN);
  }
}

auto Theme::instance() -> Theme* {
  static Theme sTheme{};
  return &sTheme;
}

}  // namespace themes
}  // namespace ui
