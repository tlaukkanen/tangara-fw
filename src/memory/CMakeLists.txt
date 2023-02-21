idf_component_register(SRCS "arena.cpp" INCLUDE_DIRS "include" REQUIRES "span")
target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
