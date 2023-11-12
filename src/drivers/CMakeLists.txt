# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "touchwheel.cpp" "i2s_dac.cpp" "gpios.cpp" "adc.cpp" "storage.cpp" "i2c.cpp"
  "spi.cpp" "display.cpp" "display_init.cpp" "samd.cpp" "relative_wheel.cpp" "wm8523.cpp"
  "nvs.cpp" "bluetooth.cpp" "haptics.cpp" "spiffs.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "esp_adc" "fatfs" "result" "lvgl" "span" "tasks" "nvs_flash" "bt" "tinyfsm" "spiffs")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
