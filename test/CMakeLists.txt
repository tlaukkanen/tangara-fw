cmake_minimum_required(VERSION 3.16)
include(../common.cmake)

idf_build_set_property(
  COMPILE_OPTIONS "-DCATCH_CONFIG_NO_POSIX_SIGNALS -DCATCH_CONFIG_FAST_COMPILE" APPEND)

# Treat warnings as errors for test purposes.
list(APPEND EXTRA_WARNINGS "-Werror")

list(APPEND EXTRA_COMPONENT_DIRS "$ENV{PROJ_PATH}/src/drivers")

# List all components that include tests here.
set(TEST_COMPONENTS "drivers")

project(device_tests)
