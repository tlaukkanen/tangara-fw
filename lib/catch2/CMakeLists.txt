# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "catch_runner.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "console")
