idf_component_register(
  SRCS "main.cpp"
  REQUIRES "drivers")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
