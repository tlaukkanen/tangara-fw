/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "nvs.hpp"
#include <stdint.h>

#include <cstdint>
#include <memory>

#include "bluetooth.hpp"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

namespace drivers {

static constexpr char kTag[] = "nvm";
static constexpr uint8_t kSchemaVersion = 1;

static constexpr char kKeyVersion[] = "ver";
static constexpr char kKeyBluetooth[] = "bt";

auto NvsStorage::Open() -> NvsStorage* {
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_LOGW(kTag, "partition needs initialisation");
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  if (err != ESP_OK) {
    ESP_LOGE(kTag, "failed to init nvm");
    return nullptr;
  }

  nvs_handle_t handle;
  if ((err = nvs_open("tangara", NVS_READWRITE, &handle)) != ESP_OK) {
    ESP_LOGE(kTag, "failed to open nvs namespace");
    return nullptr;
  }

  std::unique_ptr<NvsStorage> instance = std::make_unique<NvsStorage>(handle);
  if (instance->SchemaVersion() < kSchemaVersion) {
    ESP_LOGW(kTag, "namespace needs downgrading");
    nvs_erase_all(handle);
    nvs_set_u8(handle, kKeyVersion, kSchemaVersion);
    err = nvs_commit(handle);
    if (err != ESP_OK) {
      ESP_LOGW(kTag, "failed to init namespace");
      return nullptr;
    }
  }

  ESP_LOGI(kTag, "nvm storage initialised okay");
  return instance.release();
}

NvsStorage::NvsStorage(nvs_handle_t handle) : handle_(handle) {}

NvsStorage::~NvsStorage() {
  nvs_close(handle_);
  nvs_flash_deinit();
}

auto NvsStorage::SchemaVersion() -> uint8_t {
  uint8_t ret;
  if (nvs_get_u8(handle_, kKeyVersion, &ret) != ESP_OK) {
    return UINT8_MAX;
  }
  return ret;
}

auto NvsStorage::PreferredBluetoothDevice()
    -> std::optional<bluetooth::mac_addr_t> {
  bluetooth::mac_addr_t out{0};
  size_t size = out.size();
  if (nvs_get_blob(handle_, kKeyBluetooth, out.data(), &size) != ESP_OK) {
    return {};
  }
  return out;
}
auto NvsStorage::PreferredBluetoothDevice(
    std::optional<bluetooth::mac_addr_t> addr) -> void {
  if (!addr) {
    nvs_erase_key(handle_, kKeyBluetooth);
  } else {
    nvs_set_blob(handle_, kKeyBluetooth, addr.value().data(),
                 addr.value().size());
  }
  nvs_commit(handle_);
}

}  // namespace drivers
