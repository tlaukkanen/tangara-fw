# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "memory_resource.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "esp_psram")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
