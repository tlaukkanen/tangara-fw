# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "app_console.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "dev_console" "events" "database")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
