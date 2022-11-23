idf_component_register(
  SRCS "audio_decoder.cpp" "audio_task.cpp" "chunk.cpp" "fatfs_audio_input.cpp"
  "stream_info.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "codecs" "drivers" "cbor" "result" "tasks")

target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
