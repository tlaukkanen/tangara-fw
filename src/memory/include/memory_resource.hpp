/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory_resource>
#include <string>

#include <esp_heap_caps.h>
#include <stdint.h>

namespace memory {

enum class Capabilities : uint32_t {
  kDefault = MALLOC_CAP_DEFAULT,
  kInternal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
  kDma = MALLOC_CAP_DMA,
  kSpiRam = MALLOC_CAP_SPIRAM,
};

class Resource : public std::pmr::memory_resource {
 public:
  explicit Resource(Capabilities caps) : caps_(caps) {}

 private:
  Capabilities caps_;

  void* do_allocate(std::size_t bytes, std::size_t alignment) override;

  void do_deallocate(void* p,
                     std::size_t bytes,
                     std::size_t alignment) override;

  bool do_is_equal(
      const std::pmr::memory_resource& other) const noexcept override;
};

extern Resource kSpiRamResource;

template <typename T>
auto SpiRamAllocator() {
  return std::pmr::polymorphic_allocator<T>{&kSpiRamResource};
}

}  // namespace memory
