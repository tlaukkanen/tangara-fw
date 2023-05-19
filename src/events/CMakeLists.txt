idf_component_register(
  SRCS "event_queue.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "tinyfsm")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
