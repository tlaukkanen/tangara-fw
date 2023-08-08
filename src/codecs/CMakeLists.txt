# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "codec.cpp" "mad.cpp" "foxenflac.cpp" "opus.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "result" "span" "libmad" "libfoxenflac")

target_compile_options("${COMPONENT_LIB}" PRIVATE ${EXTRA_WARNINGS})

set(OPUS_FIXED_POINT ON)
set(OPUS_ENABLE_FLOAT_API OFF)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF)
set(OPUS_BUILD_TESTING OFF)
set(OPUS_BUILD_SHARED_LIBS OFF)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

add_subdirectory($ENV{PROJ_PATH}/lib/opus ${CMAKE_CURRENT_BINARY_DIR}/opus)
target_link_libraries(${COMPONENT_LIB} PUBLIC opus)
