# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "test_mad.cpp"
  INCLUDE_DIRS "."
  REQUIRES catch2 cmock codecs)
