idf_component_register(
  SRCS "console.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "console")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
