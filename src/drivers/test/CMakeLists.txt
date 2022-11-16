idf_component_register(
  SRCS "test_storage.cpp" "test_gpio_expander.cpp" "test_battery.cpp" "test_dac.cpp"
  INCLUDE_DIRS "." REQUIRES catch2 cmock drivers)
