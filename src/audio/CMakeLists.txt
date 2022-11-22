idf_component_register(
  SRCS "audio_decoder.cpp" "audio_task.cpp" "fatfs_audio_input.cpp" "chunk.cpp"
  "i2s_audio_output.cpp" "stream_info.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "codecs" "drivers" "cbor")

target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
