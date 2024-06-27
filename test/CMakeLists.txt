# Copyright 2024 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

cmake_minimum_required(VERSION 3.16)
include($ENV{PROJ_PATH}/tools/cmake/common.cmake)

set(SDKCONFIG_DEFAULTS "../sdkconfig.common;sdkconfig.test")

# Treat warnings as errors for test purposes.
# TODO(jacqueline): lvgl warning :(
#list(APPEND EXTRA_WARNINGS "-Werror")

list(APPEND EXTRA_COMPONENT_DIRS
  "$ENV{PROJ_PATH}/src/codecs"
  "$ENV{PROJ_PATH}/src/drivers"
  "$ENV{PROJ_PATH}/src/graphics"
  "$ENV{PROJ_PATH}/src/locale"
  "$ENV{PROJ_PATH}/src/memory"
  "$ENV{PROJ_PATH}/src/tangara"
  "$ENV{PROJ_PATH}/src/tasks"
  "$ENV{PROJ_PATH}/src/util"
  "fixtures"
  )

# List all components that include tests here.
set(TEST_COMPONENTS "drivers" "tangara")

project(device_tests)
