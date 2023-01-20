cmake_minimum_required(VERSION 3.16)
include($ENV{PROJ_PATH}/tools/cmake/common.cmake)

set(SDKCONFIG_DEFAULTS "../sdkconfig.common;sdkconfig.host")

# TODO.
#idf_build_set_property(
#  COMPILE_OPTIONS "-DCATCH_CONFIG_NO_POSIX_SIGNALS -DCATCH_CONFIG_FAST_COMPILE" APPEND)

#list(APPEND EXTRA_COMPONENT_DIRS
#  "$ENV{PROJ_PATH}/src/audio"
#  "$ENV{PROJ_PATH}/src/codecs"
#  "$ENV{PROJ_PATH}/src/tasks"
#  "$ENV{PROJ_PATH}/src/drivers"
#  "$ENV{PROJ_PATH}/src/dev_console"
#  )

set(COMPONENTS main)

project(host_app)
