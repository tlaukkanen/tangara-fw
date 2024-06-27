# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(SRCS catch2.cpp catch_runner.cpp INCLUDE_DIRS include)

target_compile_options(${COMPONENT_LIB} PUBLIC -DCATCH_CONFIG_NO_POSIX_SIGNALS -DCATCH_CONFIG_FAST_COMPILE -DCATCH_CONFIG_ENABLE_BENCHMARKING)
