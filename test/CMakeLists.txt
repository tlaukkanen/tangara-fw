cmake_minimum_required(VERSION 3.16)
include(../common.cmake)

# Treat warnings as errors for test purposes.
list(APPEND EXTRA_WARNINGS "-Werror")

# List all components that include tests here.
set(TEST_COMPONENTS "drivers")

project(device_tests)
