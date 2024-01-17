# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "system_fsm.cpp" "running.cpp" "booting.cpp" "idle.cpp"
  "service_locator.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "tinyfsm" "drivers" "database" "ui" "result" "events" "audio"
  "app_console" "battery" "locale")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
