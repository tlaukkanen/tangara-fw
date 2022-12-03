idf_component_register(
  SRCS "codec.cpp" "mad.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "result" "span")

add_dependencies("${COMPONENT_LIB}" libmad)
target_compile_options("${COMPONENT_LIB}" PRIVATE ${EXTRA_WARNINGS})
target_include_directories("${COMPONENT_LIB}" PRIVATE "${LIBMAD_INCLUDE}")
