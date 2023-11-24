# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only

idf_component_register(
  SRCS "audio_decoder.cpp" "fatfs_audio_input.cpp" "i2s_audio_output.cpp"
  "track_queue.cpp" "audio_fsm.cpp" "audio_converter.cpp" "resample.cpp"
  "fatfs_source.cpp" "bt_audio_output.cpp" "readahead_source.cpp"
  "audio_source.cpp"
  INCLUDE_DIRS "include"
  REQUIRES "codecs" "drivers" "cbor" "result" "tasks" "span" "memory" "tinyfsm"
  "database" "system_fsm" "playlist" "speexdsp")

target_compile_options(${COMPONENT_LIB} PRIVATE ${EXTRA_WARNINGS})
