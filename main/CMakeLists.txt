idf_component_register(SRCS
                       "gay-ipod-fw.cpp"
                       "dac.cpp"
                       "gpio-expander.cpp"
                       "battery.cpp"
                       "storage.cpp"
                       "i2c.cpp" INCLUDE_DIRS "." REQUIRES
                       "esp_adc_cal"
                       "fatfs"
                       "audio_pipeline"
                       "audio_stream"
                       "result")
