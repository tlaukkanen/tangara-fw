# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only
idf_component_register(
  SRCS src/info.c src/internal.c src/internal.h src/opusfile.c src/stream.c
  INCLUDE_DIRS "include"
  REQUIRES "ogg")
target_compile_options("${COMPONENT_LIB}" PRIVATE "-DOP_FIXED_POINT" "-Wno-maybe-uninitialized")

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

set(OPUS_FIXED_POINT ON)
set(OPUS_ENABLE_FLOAT_API OFF)

set(OPUS_VAR_ARRAYS OFF)
set(OPUS_USE_ALLOCA ON)
set(OPUS_NONTHREADSAFE_PSEUDOSTACK OFF)

set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF)
set(OPUS_BUILD_TESTING OFF)
set(OPUS_BUILD_SHARED_LIBS OFF)

add_subdirectory($ENV{PROJ_PATH}/lib/opus ${CMAKE_CURRENT_BINARY_DIR}/opus)
target_compile_definitions(opus PRIVATE CUSTOM_SUPPORT)
target_compile_options(opus PRIVATE -Os -DSMALL_FOOTPRINT -funroll-loops -ffast-math)
target_include_directories(opus PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${COMPONENT_LIB} PUBLIC opus)
