#pragma once

#include <cstddef>
#include <cstdint>

#include "esp32/himem.h"
#include "span.hpp"

/*
 * Wrapper around an ESP-IDF himem allocation, which uses RAII to clean up after
 * itself.
 */
template <std::size_t size>
class HimemAlloc {
 public:
  esp_himem_handle_t handle;
  const bool is_valid;

  HimemAlloc() : is_valid(esp_himem_alloc(size, &handle) == ESP_OK) {}

  ~HimemAlloc() {
    if (is_valid) {
      esp_himem_free(handle);
    }
  }

  // Not copyable or movable.
  HimemAlloc(const HimemAlloc&) = delete;
  HimemAlloc& operator=(const HimemAlloc&) = delete;
};

/*
 * Wrapper around an ESP-IDF himem allocation, which maps a HimemAlloc into the
 * usable address space. Instances always contain the last memory region that
 * was mapped within them.
 */
template <std::size_t size>
class MappableRegion {
 private:
  std::byte* bytes_;

 public:
  esp_himem_rangehandle_t range_handle;
  const bool is_valid;

  MappableRegion()
      : bytes_(nullptr),
        is_valid(esp_himem_alloc_map_range(size, &range_handle) == ESP_OK) {}

  ~MappableRegion() {
    if (bytes_ != nullptr) {
      esp_himem_unmap(range_handle, bytes_, size);
    }
    if (is_valid) {
      esp_himem_free_map_range(range_handle);
    }
  }

  auto Get() -> cpp::span<std::byte> {
    if (bytes_ != nullptr) {
      return {};
    }
    return {bytes_, size};
  }

  auto Map(const HimemAlloc<size>& alloc) -> cpp::span<std::byte> {
    if (bytes_ != nullptr) {
      ESP_ERROR_CHECK(esp_himem_unmap(range_handle, bytes_, size));
    }
    ESP_ERROR_CHECK(esp_himem_map(alloc.handle, range_handle, 0, 0, size, 0,
                                  reinterpret_cast<void**>(&bytes_)));
    return Get();
  }

  // Not copyable or movable.
  MappableRegion(const MappableRegion&) = delete;
  MappableRegion& operator=(const MappableRegion&) = delete;
};
