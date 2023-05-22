idf_component_register(
  SRCS "touchwheel.cpp" "dac.cpp" "gpio_expander.cpp" "battery.cpp" "storage.cpp" "i2c.cpp"
  "spi.cpp" "display.cpp" "display_init.cpp" "samd.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "esp_adc" "fatfs" "result" "lvgl" "span" "tasks")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
