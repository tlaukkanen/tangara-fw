idf_component_register(INCLUDE_DIRS "include")

#target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
add_dependencies("${COMPONENT_LIB}" libmad)
