# Copyright 2024 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRC_DIRS "app_console" "audio" "battery" "database" "dev_console" "events"
  "input" "lua" "system_fsm" "ui"
  INCLUDE_DIRS "."
  REQUIRES "codecs" "drivers" "locale" "memory" "tasks" "util" "graphics"
  "tinyfsm" "lvgl" "esp_timer" "luavgl" "esp_app_format" "libcppbor" "libtags"
  "komihash" "result" "esp_psram" "fatfs" "millershuffle" "speexdsp" "console"
  "esp-idf-lua" "lua-linenoise" "lua-term")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})

set(LEVELDB_BUILD_TESTS OFF)
set(LEVELDB_BUILD_BENCHMARKS OFF)
set(LEVELDB_INSTALL OFF)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

add_subdirectory($ENV{PROJ_PATH}/lib/leveldb ${CMAKE_CURRENT_BINARY_DIR}/leveldb)
target_link_libraries(${COMPONENT_LIB} PUBLIC leveldb)
