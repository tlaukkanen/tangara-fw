# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "lvgl_task.cpp" "ui_fsm.cpp" "screen_splash.cpp" "encoder_input.cpp"
  "themes.cpp" "widget_top_bar.cpp" "screen.cpp" "modal_progress.cpp"
  "modal.cpp" "modal_confirm.cpp" "screen_settings.cpp" "event_binding.cpp"
  "screen_lua.cpp"
  "splash.c" "font_fusion_12.c" "font_fusion_10.c"
  INCLUDE_DIRS "include"
  REQUIRES "drivers" "lvgl" "tinyfsm" "events" "system_fsm" "database" "esp_timer" "battery" "bindey" "lua" "luavgl" "esp_app_format")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
