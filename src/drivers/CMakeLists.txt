idf_component_register(
  SRCS "dac.cpp" "gpio_expander.cpp" "battery.cpp" "storage.cpp" "i2c.cpp"
  "spi.cpp" "display.cpp" "display_init.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "esp_adc_cal" "fatfs" "result" "lvgl" "span")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
