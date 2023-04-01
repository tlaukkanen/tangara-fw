idf_component_register(
  SRCS "audio_decoder.cpp" "audio_task.cpp" "chunk.cpp" "fatfs_audio_input.cpp"
  "stream_message.cpp" "i2s_audio_output.cpp" "stream_buffer.cpp"
  "audio_playback.cpp" "stream_event.cpp" "pipeline.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "codecs" "drivers" "cbor" "result" "tasks" "span" "memory")

target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
