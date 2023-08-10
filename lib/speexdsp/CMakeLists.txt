# Copyright 2023 jacqueline <me@jacqueline.id.au>
#
# SPDX-License-Identifier: GPL-3.0-only
idf_component_register(
  SRCS libspeexdsp/buffer.c libspeexdsp/fftwrap.c libspeexdsp/filterbank.c
libspeexdsp/jitter.c libspeexdsp/kiss_fft.c libspeexdsp/kiss_fftr.c
libspeexdsp/mdf.c libspeexdsp/preprocess.c libspeexdsp/resample.c
libspeexdsp/scal.c libspeexdsp/smallft.c
  INCLUDE_DIRS "include"
  PRIV_INCLUDE_DIRS "libspeexdsp"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -DHAVE_CONFIG_H -Wno-shift-negative-value -Wno-unused-const-variable)
