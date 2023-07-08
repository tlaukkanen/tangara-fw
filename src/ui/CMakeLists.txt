# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "lvgl_task.cpp" "ui_fsm.cpp" "screen_splash.cpp" "screen_menu.cpp" "wheel_encoder.cpp" "screen_track_browser.cpp" "screen_playing.cpp" "themes.cpp" "splash.c"
  INCLUDE_DIRS "include"
  REQUIRES "drivers" "lvgl" "tinyfsm" "events" "system_fsm" "database" "esp_timer")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
