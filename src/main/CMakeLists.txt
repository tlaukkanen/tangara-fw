idf_component_register(
  SRCS "main.cpp" "app_console.cpp"
  INCLUDE_DIRS "."
  REQUIRES "audio" "drivers" "dev_console" "ui")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
