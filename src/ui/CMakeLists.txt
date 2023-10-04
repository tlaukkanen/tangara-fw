# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "lvgl_task.cpp" "ui_fsm.cpp" "screen_splash.cpp" "screen_menu.cpp"
  "encoder_input.cpp" "screen_track_browser.cpp" "screen_playing.cpp"
  "themes.cpp" "widget_top_bar.cpp" "screen.cpp" "screen_onboarding.cpp"
  "modal_progress.cpp" "modal.cpp" "modal_confirm.cpp" "screen_settings.cpp"
  "event_binding.cpp" "modal_add_to_queue.cpp"
  "splash.c" "font_fusion.c" "font_symbols.c"
  "icons/battery_empty.c" "icons/battery_full.c" "icons/battery_20.c"
  "icons/battery_40.c" "icons/battery_60.c" "icons/battery_80.c" "icons/play.c"
  "icons/pause.c" "icons/bluetooth.c"
  INCLUDE_DIRS "include"
  REQUIRES "drivers" "lvgl" "tinyfsm" "events" "system_fsm" "database" "esp_timer" "battery" "bindey")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
