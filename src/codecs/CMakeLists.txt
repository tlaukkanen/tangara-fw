# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "codec.cpp" "mad.cpp" "foxenflac.cpp" "opus.cpp" "vorbis.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "result" "span" "libmad" "libfoxenflac" "tremor" "opusfile")

target_compile_options("${COMPONENT_LIB}" PRIVATE ${EXTRA_WARNINGS})
