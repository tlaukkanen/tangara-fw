# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRC_DIRS "battery"
  INCLUDE_DIRS "." REQUIRES catch2 cmock tangara fixtures)
