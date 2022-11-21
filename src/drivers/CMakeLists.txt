idf_component_register(
  SRCS "dac.cpp" "gpio_expander.cpp" "battery.cpp" "storage.cpp" "i2c.cpp"
  "audio_playback.cpp" "i2s_audio_output.cpp" "display.cpp" "display_init.cpp" "spi.cpp" 
  "trackpad.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "esp_adc_cal" "fatfs" "audio_pipeline" "audio_stream" "result" "lvgl")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
