# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only
idf_component_register(SRCS "src/luavgl.c" INCLUDE_DIRS "src" REQUIRES "esp-idf-lua" "lvgl")

