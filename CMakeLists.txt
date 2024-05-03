# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only
cmake_minimum_required(VERSION 3.16)

include($ENV{PROJ_PATH}/tools/cmake/common.cmake)

set(SDKCONFIG_DEFAULTS "sdkconfig.common")

# No exceptions in app builds (this is different in test builds).
idf_build_set_property(COMPILE_OPTIONS "-DRESULT_DISABLE_EXCEPTIONS" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DTCB_SPAN_NO_CONTRACT_CHECKING" APPEND)

# Include all app components.
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{PROJ_PATH}/src")

project(tangara)

# 'collate' partition on internal flash. Contains collation data for sorting
# strings across languages (defaults to a generic dataset based on ISO14651).
get_filename_component(collate_full_path "tools/collate/Generic.LC_COLLATE" ABSOLUTE)
esptool_py_flash_to_partition(flash "collate" ${collate_full_path})

# /lua partition on internal flash, for storing the lua application
spiffs_create_partition_image(lua lua FLASH_IN_PROJECT)

# /repl partition on internal flash, for storing the developer repl and its deps
file(COPY lib/lua-repl/repl DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/repl)
file(COPY lib/lua-term/term DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/repl)
spiffs_create_partition_image(repl ${CMAKE_CURRENT_BINARY_DIR}/repl FLASH_IN_PROJECT)

add_custom_target(check-for-sdkconfig-changes ALL
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/cmake/check-for-sdkconfig-changes.sh
)
