# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "collation.cpp" "strxfrm_l.c"
  INCLUDE_DIRS "include"
  PRIV_INCLUDE_DIRS "priv_include"
  REQUIRES "esp_partition" "spi_flash")

target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
