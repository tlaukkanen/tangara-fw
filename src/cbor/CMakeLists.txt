idf_component_register(
  SRCS "cbor_decoder.cpp" "cbor_encoder.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "cbor" "result")

target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
