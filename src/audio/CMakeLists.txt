# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "audio_task.cpp" "chunk.cpp" "fatfs_audio_input.cpp"
  "stream_message.cpp" "i2s_audio_output.cpp" "stream_buffer.cpp" "track_queue.cpp"
  "stream_event.cpp" "stream_info.cpp" "audio_fsm.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "codecs" "drivers" "cbor" "result" "tasks" "span" "memory" "tinyfsm" "database" "system_fsm" "playlist")

target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
