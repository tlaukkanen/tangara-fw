// Copyright 2023 jacqueline <me@jacqueline.id.au>
//
// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "esp_heap_caps.h"

#define FIXED_POINT
#define EXPORT
#define STDC_HEADERS
#define USE_SMALLFT

#define DISABLE_ENCODER
#define DISABLE_FLOAT_API

#define OVERRIDE_SPEEX_ALLOC
#define OVERRIDE_SPEEX_REALLOC
#define OVERRIDE_SPEEX_FREE

static inline void* speex_alloc(int size) {
  return heap_caps_calloc(size, 1, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static inline void* speex_realloc(void* ptr, int size) {
  return heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static inline void speex_free(void* ptr) {
  heap_caps_free(ptr);
}
