# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "touchwheel.cpp" "i2s_dac.cpp" "gpios.cpp" "adc.cpp" "storage.cpp"
  "i2c.cpp" "bluetooth.cpp" "spi.cpp" "display.cpp" "display_init.cpp"
  "samd.cpp" "wm8523.cpp" "nvs.cpp" "haptics.cpp" "spiffs.cpp" "pcm_buffer.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "esp_adc" "fatfs" "result" "lvgl" "nvs_flash" "spiffs" "bt"
  "tasks" "tinyfsm" "util" "libcppbor" "driver")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
