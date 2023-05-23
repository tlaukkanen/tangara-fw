/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

cmake_minimum_required(VERSION 3.16)
include($ENV{PROJ_PATH}/tools/cmake/common.cmake)

set(SDKCONFIG_DEFAULTS "../sdkconfig.common;sdkconfig.test")

idf_build_set_property(
  COMPILE_OPTIONS "-DCATCH_CONFIG_NO_POSIX_SIGNALS -DCATCH_CONFIG_FAST_COMPILE" APPEND)

# Treat warnings as errors for test purposes.
# TODO(jacqueline): lvgl warning :(
#list(APPEND EXTRA_WARNINGS "-Werror")

list(APPEND EXTRA_COMPONENT_DIRS
  "$ENV{PROJ_PATH}/src/audio"
  "$ENV{PROJ_PATH}/src/codecs"
  "$ENV{PROJ_PATH}/src/database"
  "$ENV{PROJ_PATH}/src/drivers"
  "$ENV{PROJ_PATH}/src/memory"
  "$ENV{PROJ_PATH}/src/tasks"
  "$ENV{PROJ_PATH}/src/ui"
  "$ENV{PROJ_PATH}/src/dev_console"
  "fixtures"
  )

# List all components that include tests here.
set(TEST_COMPONENTS "codecs" "database" "drivers")

project(device_tests)
