idf_component_register(
  SRCS "gay-ipod-fw.cpp" "gpio-expander.cpp" "battery.cpp" "storage.cpp"
  INCLUDE_DIRS "."
  REQUIRES "esp_adc_cal" "fatfs")
