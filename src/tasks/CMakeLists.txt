idf_component_register(SRCS "tasks.cpp" INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
