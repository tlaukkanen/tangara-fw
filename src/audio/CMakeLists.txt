idf_component_register(
  SRCS "audio_decoder.cpp" "fatfs_audio_input.cpp" "audio_task.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "codecs" "drivers")

target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
