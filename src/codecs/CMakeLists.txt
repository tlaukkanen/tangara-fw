idf_component_register(
  SRCS "codec.cpp" "mad.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "result" "span" "libmad")

target_compile_options("${COMPONENT_LIB}" PRIVATE ${EXTRA_WARNINGS})
