# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "lua_thread.cpp" "bridge.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "drivers" "lvgl" "tinyfsm" "events" "system_fsm" "database" "esp_timer" "battery" "esp-idf-lua" "luavgl")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
