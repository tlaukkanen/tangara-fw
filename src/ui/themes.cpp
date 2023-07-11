#include "themes.hpp"
#include "core/lv_obj.h"
#include "misc/lv_color.h"

LV_FONT_DECLARE(font_fusion);

namespace ui {
namespace themes {

static void theme_apply_cb(lv_theme_t* th, lv_obj_t* obj) {
  reinterpret_cast<Theme*>(th->user_data)->Callback(obj);
}

Theme::Theme() {
  /*Initialize the styles*/
  lv_style_init(&button_style_);
  lv_style_set_bg_color(&button_style_, lv_palette_main(LV_PALETTE_GREEN));
  lv_style_set_border_color(&button_style_,
                            lv_palette_darken(LV_PALETTE_GREEN, 3));
  lv_style_set_border_width(&button_style_, 1);

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
  lv_obj_set_style_text_font(obj, &font_fusion, 0);
  lv_obj_set_style_text_color(obj, lv_color_black(), 0);

  if (lv_obj_check_type(obj, &lv_btn_class) ||
      lv_obj_check_type(obj, &lv_list_btn_class)) {
    lv_obj_add_style(obj, &button_style_, LV_PART_MAIN | LV_STATE_FOCUSED);
  }
}
}  // namespace themes
}  // namespace ui
