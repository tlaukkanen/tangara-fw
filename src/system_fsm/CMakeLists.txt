idf_component_register(
  SRCS "system_fsm.cpp" "running.cpp" "booting.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "tinyfsm" "drivers" "database" "ui" "result" "events" "audio")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
