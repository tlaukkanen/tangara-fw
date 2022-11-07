# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ESP-ADF clobbers EXTRA_COMPONENT_DIRS, so include it first.
include($ENV{ADF_PATH}/CMakeLists.txt)

# Build only the subset of components that we actually depend on.
set(COMPONENTS "")

# External dependencies
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{PROJ_PATH}/lib/result")
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{PROJ_PATH}/lib/lvgl")

# Project components
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{PROJ_PATH}/src")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Additional warnings used when compiling our components.
# Unable to be used due to issues in ESP-IDF includes are: -Wpedantic
# -Wuseless-cast -Wconversion -Wold-style-cast -Wsign-conversion -Wcast-align
set(EXTRA_WARNINGS "-Wshadow" "-Wnon-virtual-dtor" "-Wunused"
  "-Woverloaded-virtual" "-Wmisleading-indentation" "-Wduplicated-cond"
  "-Wduplicated-branches" "-Wlogical-op" "-Wnull-dereference"
  "-Wdouble-promotion" "-Wformat=2" "-Wimplicit-fallthrough")

# Extra build flags that should apply to the entire build. This should mostly
# just be used to setting flags that our external dependencies requires.
# Otherwise, prefer adding per-component build flags to keep things neat.
idf_build_set_property(
  COMPILE_OPTIONS "-DRESULT_DISABLE_EXCEPTIONS -DLV_CONF_INCLUDE_SIMPLE" APPEND)
