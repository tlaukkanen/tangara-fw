# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only
idf_component_register(
  SRCS bitwise.c codebook.c dsp.c floor0.c floor1.c floor_lookup.c framing.c
  info.c mapping0.c mdct.c misc.c res012.c vorbisfile.c
  INCLUDE_DIRS ".")
target_compile_options("${COMPONENT_LIB}" PRIVATE -Wno-error=misleading-indentation -Wno-error=maybe-uninitialized -Wno-error=char-subscripts -Wno-error=unused-label)
