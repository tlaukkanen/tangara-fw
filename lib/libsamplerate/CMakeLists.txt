# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only
idf_component_register()

set(LIBSAMPLERATE_EXAMPLES false)
set(LIBSAMPLERATE_INSTALL false)
set(BUILD_TESTING false)

add_subdirectory(libsamplerate-0.2.2)
target_link_libraries(${COMPONENT_LIB} INTERFACE samplerate)
