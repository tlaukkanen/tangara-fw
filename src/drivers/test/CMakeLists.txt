# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "test_adc.cpp" "test_storage.cpp" "test_dac.cpp" "test_samd.cpp"
  INCLUDE_DIRS "." REQUIRES catch2 cmock drivers fixtures)
