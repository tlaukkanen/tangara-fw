# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only
idf_component_register(
  SRCS block.c codebook.c floor0.c floor1.c info.c mapping0.c mdct.c registry.c
  res012.c sharedbook.c synthesis.c vorbisfile.c window.c
  INCLUDE_DIRS "."
  REQUIRES "ogg")

target_compile_options("${COMPONENT_LIB}" PRIVATE -Og)
