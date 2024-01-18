# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "codec.cpp" "mad.cpp" "miniflac.cpp" "opus.cpp" "vorbis.cpp"
       "source_buffer.cpp" "sample.cpp" "wav.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "result" "span" "libmad" "miniflac" "tremor" "opusfile" "memory" "util"
       "komihash")

target_compile_options("${COMPONENT_LIB}" PRIVATE ${EXTRA_WARNINGS})
