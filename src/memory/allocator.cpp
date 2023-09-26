/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <cstdint>

#include "esp_heap_caps.h"

void* operator new[](std::size_t sz) {
  if (sz == 0) {
    ++sz;  // avoid std::malloc(0) which may return nullptr on success
  }

  if (sz > 256) {
    return heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
  }

  return heap_caps_malloc(sz, MALLOC_CAP_DEFAULT);
}

void operator delete[](void* ptr) noexcept {
  heap_caps_free(ptr);
}

void operator delete[](void* ptr, std::size_t size) noexcept {
  heap_caps_free(ptr);
}