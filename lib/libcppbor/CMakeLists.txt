# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only
idf_component_register(
  SRCS cppbor.cpp cppbor_parse.cpp
  INCLUDE_DIRS "include/cppbor"
)
target_compile_options(${COMPONENT_LIB} PRIVATE
  "-Wno-shadow" "-Wno-deprecated-enum-enum-conversion")
