idf_component_register(
  SRCS "lvgl_task.cpp" "ui_fsm.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "drivers" "lvgl" "tinyfsm" "events" "system_fsm" "database")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
