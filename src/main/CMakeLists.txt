idf_component_register(
  SRCS "main.cpp" "app_console.cpp"
  INCLUDE_DIRS "."
  REQUIRES "drivers" "dev_console")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
