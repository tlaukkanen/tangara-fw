# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only
idf_component_register(SRCS dr_flac.c INCLUDE_DIRS .)
target_compile_options("${COMPONENT_LIB}" PRIVATE -Ofast)
