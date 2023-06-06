# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "touchwheel.cpp" "i2s_dac.cpp" "gpio_expander.cpp" "battery.cpp" "storage.cpp" "i2c.cpp"
  "spi.cpp" "display.cpp" "display_init.cpp" "samd.cpp" "relative_wheel.cpp" "digital_pot.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "esp_adc" "fatfs" "result" "lvgl" "span" "tasks")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
