cmake_minimum_required(VERSION 3.16)
include($ENV{PROJ_PATH}/tools/cmake/common.cmake)

set(SDKCONFIG_DEFAULTS "sdkconfig.common")

# No exceptions in app builds (this is different in test builds).
idf_build_set_property(COMPILE_OPTIONS "-DRESULT_DISABLE_EXCEPTIONS" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DTCB_SPAN_NO_CONTRACT_CHECKING" APPEND)

# Include all app components.
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{PROJ_PATH}/src")

project(tangara)
