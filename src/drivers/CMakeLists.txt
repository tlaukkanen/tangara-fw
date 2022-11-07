idf_component_register(
  SRCS "dac.cpp" "gpio-expander.cpp" "battery.cpp" "storage.cpp" "i2c.cpp"
  "playback.cpp" "display.cpp" "display-init.cpp" 
  INCLUDE_DIRS "include"
  REQUIRES "esp_adc_cal" "fatfs" "audio_pipeline" "audio_stream" "result" "lvgl")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
