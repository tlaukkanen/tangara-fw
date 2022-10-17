# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.8)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include($ENV{ADF_PATH}/CMakeLists.txt)
list(APPEND EXTRA_COMPONENT_DIRS "lib/result")
list(APPEND EXTRA_COMPONENT_DIRS "lib/lvgl")

set(ENV{EXTRA_CPPFLAGS} "-DRESULT_DISABLE_EXCEPTIONS -DLV_CONF_INCLUDE_SIMPLE")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(gay-ipod-fw)
