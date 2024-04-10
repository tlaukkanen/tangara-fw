# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "input_touch_wheel.cpp" "input_touch_dpad.cpp"
  "input_volume_buttons.cpp" "lvgl_input_driver.cpp" "feedback_haptics.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "drivers" "lvgl" "events" "system_fsm")

target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
