#include "themes.hpp"
#include "core/lv_obj.h"
#include "core/lv_obj_tree.h"
#include "esp_log.h"
#include "misc/lv_color.h"
#include "misc/lv_style.h"
#include "widgets/lv_slider.h"

LV_FONT_DECLARE(font_fusion_12);

namespace ui {
namespace themes {

static void theme_apply_cb(lv_theme_t* th, lv_obj_t* obj) {
  reinterpret_cast<Theme*>(th->user_data)->Callback(obj);
}

Theme::Theme() {
  /*Initialize the styles*/
  lv_style_init(&button_style_);
  lv_style_set_pad_left(&button_style_, 2);
  lv_style_set_pad_right(&button_style_, 2);
  lv_style_set_pad_top(&button_style_, 1);
  lv_style_set_pad_bottom(&button_style_, 1);
  lv_style_set_bg_color(&button_style_, lv_color_white());
  lv_style_set_radius(&button_style_, 5);

  lv_style_init(&button_style_focused_);
  lv_style_set_bg_color(&button_style_focused_,
                        lv_palette_lighten(LV_PALETTE_BLUE, 5));
  lv_style_set_bg_opa(&button_style_focused_, LV_OPA_COVER);

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
  lv_obj_set_style_text_font(obj, &font_fusion_12, 0);
  lv_obj_set_style_text_color(obj, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_bg_color(obj, lv_palette_lighten(LV_PALETTE_BLUE, 5),
                            LV_PART_SELECTED | LV_STATE_CHECKED);

  if (lv_obj_check_type(obj, &lv_btn_class) ||
      lv_obj_check_type(obj, &lv_list_btn_class)) {
    lv_obj_add_style(obj, &button_style_, LV_PART_MAIN);
    lv_obj_add_style(obj, &button_style_focused_,
                     LV_PART_MAIN | LV_STATE_FOCUSED);
    if (lv_obj_check_type(obj, &lv_list_btn_class)) {
      lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_left(obj, 1, LV_PART_MAIN);
      lv_obj_set_style_pad_right(obj, 1, LV_PART_MAIN);
    }
  } else if (lv_obj_check_type(obj, &lv_slider_class)) {
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);

    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(obj, lv_palette_main(LV_PALETTE_BLUE),
                              LV_PART_INDICATOR);

    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(obj, 2, LV_PART_KNOB);
    lv_obj_set_style_bg_color(obj, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_bg_color(obj, lv_palette_lighten(LV_PALETTE_BLUE, 4),
                              LV_PART_KNOB | LV_STATE_FOCUSED);
    lv_obj_set_style_shadow_width(obj, 5, LV_PART_KNOB);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_COVER, LV_PART_KNOB);
  } else if (lv_obj_check_type(obj, &lv_switch_class)) {
    lv_obj_set_size(obj, 28, 18);
    lv_obj_set_style_pad_all(obj, -2, LV_PART_KNOB);

    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);

    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(obj, lv_palette_main(LV_PALETTE_GREY),
                              LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(obj, lv_palette_main(LV_PALETTE_BLUE),
                              LV_PART_INDICATOR | LV_STATE_CHECKED);

    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(obj, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_bg_color(obj, lv_palette_lighten(LV_PALETTE_BLUE, 4),
                              LV_PART_KNOB | LV_STATE_FOCUSED);
  } else if (lv_obj_check_type(obj, &lv_dropdown_class)) {
    lv_obj_set_style_radius(obj, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj, 2, LV_PART_MAIN);

    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, lv_palette_main(LV_PALETTE_BLUE),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_FULL, LV_PART_MAIN);

    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, lv_palette_lighten(LV_PALETTE_BLUE, 5),
                              LV_PART_MAIN | LV_STATE_FOCUSED);
  }
}

void Theme::ApplyStyle(lv_obj_t* obj, Style style) {
  switch (style) {
    case Style::kTopBar:
      lv_obj_set_style_pad_bottom(obj, 1, LV_PART_MAIN);

      lv_obj_set_style_shadow_width(obj, 6, LV_PART_MAIN);
      lv_obj_set_style_shadow_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_shadow_ofs_x(obj, 0, LV_PART_MAIN);
      break;
    case Style::kPopup:
      lv_obj_set_style_shadow_width(obj, 6, LV_PART_MAIN);
      lv_obj_set_style_shadow_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_shadow_ofs_x(obj, 0, LV_PART_MAIN);
      lv_obj_set_style_shadow_ofs_y(obj, 0, LV_PART_MAIN);

      lv_obj_set_style_radius(obj, 5, LV_PART_MAIN);

      lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_bg_color(obj, lv_color_white(), LV_PART_MAIN);

      lv_obj_set_style_pad_top(obj, 2, LV_PART_MAIN);
      lv_obj_set_style_pad_bottom(obj, 2, LV_PART_MAIN);
      lv_obj_set_style_pad_left(obj, 2, LV_PART_MAIN);
      lv_obj_set_style_pad_right(obj, 2, LV_PART_MAIN);
      break;
    case Style::kTab:
      lv_obj_set_style_radius(obj, 0, LV_PART_MAIN);

      lv_obj_set_style_border_width(obj, 1, LV_STATE_CHECKED);
      lv_obj_set_style_border_color(obj, lv_palette_main(LV_PALETTE_BLUE),
                                    LV_STATE_CHECKED);
      lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_BOTTOM,
                                   LV_STATE_CHECKED);
      break;
    case Style::kButtonPrimary:
      lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
      lv_obj_set_style_border_color(obj, lv_palette_main(LV_PALETTE_BLUE),
                                    LV_PART_MAIN);
      lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_FULL, LV_PART_MAIN);
      break;
    case Style::kMenuSubheadFirst:
    case Style::kMenuSubhead:
      lv_obj_set_style_text_color(obj, lv_palette_darken(LV_PALETTE_GREY, 3),
                                  LV_PART_MAIN);
      lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

      lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
      lv_obj_set_style_border_color(obj, lv_palette_lighten(LV_PALETTE_GREY, 3),
                                    LV_PART_MAIN);

      if (style == Style::kMenuSubhead) {
        lv_obj_set_style_border_side(
            obj, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
      } else {
        lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);
      }
      break;
    default:
      break;
  }
}

auto Theme::instance() -> Theme* {
  static Theme sTheme{};
  return &sTheme;
}

}  // namespace themes
}  // namespace ui
