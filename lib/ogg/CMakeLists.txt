# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only
idf_component_register(
  SRCS src/bitwise.c src/framing.c src/crctable.h
  INCLUDE_DIRS "include")
