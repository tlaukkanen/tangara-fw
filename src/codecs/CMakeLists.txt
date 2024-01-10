# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "codec.cpp" "mad.cpp" "miniflac.cpp" "opus.cpp" "vorbis.cpp"
       "source_buffer.cpp" "sample.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "result" "span" "libmad" "miniflac" "tremor" "opusfile" "memory"
       "komihash")

target_compile_options("${COMPONENT_LIB}" PRIVATE ${EXTRA_WARNINGS})

set(FAAD_BUNDLED_MODE ON)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

add_subdirectory($ENV{PROJ_PATH}/lib/faad2 ${CMAKE_CURRENT_BINARY_DIR}/faad2)

target_link_libraries(${COMPONENT_LIB} PUBLIC faad_fixed)
