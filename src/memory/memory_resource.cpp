/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "memory_resource.hpp"

#include <memory_resource>
#include <string>
#include <utility>

#include <esp_heap_caps.h>
#include <stdint.h>

namespace memory {

Resource kSpiRamResource{Capabilities::kSpiRam};

void* Resource::do_allocate(std::size_t bytes, std::size_t alignment) {
  return heap_caps_malloc(bytes, std::to_underlying(caps_));
}

void Resource::do_deallocate(void* p,
                             std::size_t bytes,
                             std::size_t alignment) {
  heap_caps_free(p);
}

bool Resource::do_is_equal(const std::pmr::memory_resource& other) const {
  return this == &other;
}

}  // namespace memory
