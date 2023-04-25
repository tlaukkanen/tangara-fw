idf_component_register(
  SRCS "lvgl_task.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "drivers")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
