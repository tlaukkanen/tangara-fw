# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "env_esp.cpp" "database.cpp" "track.cpp" "records.cpp" "file_gatherer.cpp" "tag_parser.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "result" "span" "esp_psram" "fatfs" "libtags" "komihash" "cbor" "tasks")

target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})

set(LEVELDB_BUILD_TESTS OFF)
set(LEVELDB_BUILD_BENCHMARKS OFF)
set(LEVELDB_INSTALL OFF)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

add_subdirectory($ENV{PROJ_PATH}/lib/leveldb ${CMAKE_CURRENT_BINARY_DIR}/leveldb)

target_link_libraries(${COMPONENT_LIB} PUBLIC leveldb)
