# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "event_queue.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "tinyfsm")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
