/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "collation.hpp"

#include <cstdint>
#include <cstring>
#include <memory>

#include "esp_flash_spi_init.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "hal/spi_flash_types.h"
#include "spi_flash_mmap.h"
#include "strxfrm.h"

namespace locale {

static constexpr char kTag[] = "collate";

static constexpr esp_partition_type_t kLocalePartitionType =
    static_cast<esp_partition_type_t>(0x40);
static constexpr esp_partition_subtype_t kLcCollateSubtype =
    static_cast<esp_partition_subtype_t>(0x0);

auto CreateCollator() -> std::unique_ptr<ICollator> {
  std::unique_ptr<ICollator> ret{GLibCollator::create()};
  if (!ret) {
    ret.reset(new NoopCollator());
  }
  return ret;
}

auto GLibCollator::create() -> GLibCollator* {
  uint32_t data_pages = spi_flash_mmap_get_free_pages(SPI_FLASH_MMAP_DATA);
  ESP_LOGI(kTag, "free data pages: %lu (%lu KiB)", data_pages, data_pages * 64);

  const esp_partition_t* partition =
      esp_partition_find_first(kLocalePartitionType, kLcCollateSubtype, NULL);
  if (partition == NULL) {
    ESP_LOGW(kTag, "no LC_COLLATE partition found");
  }

  ESP_LOGI(kTag, "found LC_COLLATE partition of size %lu KiB",
           partition->size / 1024L);
  if (partition->size > data_pages * 64 * 1024) {
    ESP_LOGE(kTag, "not enough free pages to map LC_COLLATE partition!");
    return nullptr;
  }

  const void* region;
  esp_partition_mmap_handle_t handle;
  esp_err_t err = esp_partition_mmap(partition, 0, partition->size,
                                     ESP_PARTITION_MMAP_DATA, &region, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "LC_COLLATE mmap failed");
    return nullptr;
  }

  // We reserve the first 8 bytes of the partition for an identifier / name.
  // Copy it out, then crop the rest of the region so that the LC_COLLATE parser
  // doesn't see it.
  const char* region_as_str = static_cast<const char*>(region);
  std::string name{region_as_str, strnlen(region_as_str, 8)};
  region = static_cast<const std::byte*>(region) + 8;

  auto data = std::make_unique<locale_data_t>();
  if (!parse_locale_data(region, partition->size - 8, data.get())) {
    ESP_LOGE(kTag, "parsing locale data failed");
    esp_partition_munmap(handle);
    return nullptr;
  }

  return new GLibCollator(name, handle, std::move(data));
}

GLibCollator::GLibCollator(const std::string& name,
                           const esp_partition_mmap_handle_t handle,
                           std::unique_ptr<locale_data_t> locale)
    : name_(name), handle_(handle), locale_data_(std::move(locale)) {}

GLibCollator::~GLibCollator() {
  esp_partition_munmap(handle_);
}

auto GLibCollator::Transform(const std::string& in) -> std::string {
  size_t size = glib_strxfrm(NULL, in.c_str(), 0, locale_data_.get());
  char* dest = new char[size + 1]{0};
  glib_strxfrm(dest, in.c_str(), size, locale_data_.get());
  std::string out{dest, strnlen(dest, size)};
  delete[] dest;
  return out;
}

}  // namespace locale
