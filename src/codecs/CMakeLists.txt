idf_component_register(
  SRCS "codec.cpp" "mad.cpp"
  INCLUDE_DIRS "include")

target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
add_dependencies("${COMPONENT_LIB}" libmad)
