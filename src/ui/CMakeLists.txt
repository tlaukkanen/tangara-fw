# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "lvgl_task.cpp" "ui_fsm.cpp" "screen_splash.cpp" "themes.cpp"
  "screen.cpp" "modal.cpp"  "screen_lua.cpp" "splash.c" "font_fusion_12.c"
  "font_fusion_10.c"
  INCLUDE_DIRS "include"
  REQUIRES "drivers" "lvgl" "tinyfsm" "events" "system_fsm" "database" "esp_timer" "battery" "lua" "luavgl" "esp_app_format" "input")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
