/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "nvs.hpp"
#include <stdint.h>
#include <sys/_stdint.h>

#include <cstdint>
#include <memory>

#include "bluetooth.hpp"
#include "bluetooth_types.hpp"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "tasks.hpp"
#include "wm8523.hpp"

namespace drivers {

static constexpr char kTag[] = "nvm";
static constexpr uint8_t kSchemaVersion = 1;

static constexpr char kKeyVersion[] = "ver";
static constexpr char kKeyBluetooth[] = "bt";
static constexpr char kKeyOutput[] = "out";
static constexpr char kKeyBrightness[] = "bright";
static constexpr char kKeyAmpMaxVolume[] = "hp_vol_max";
static constexpr char kKeyAmpCurrentVolume[] = "hp_vol";

auto NvsStorage::OpenSync() -> NvsStorage* {
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

  std::unique_ptr<NvsStorage> instance = std::make_unique<NvsStorage>(
      std::unique_ptr<tasks::Worker>(
          tasks::Worker::Start<tasks::Type::kNvsWriter>()),
      handle);
  if (instance->SchemaVersionSync() < kSchemaVersion &&
      !instance->DowngradeSchemaSync()) {
    ESP_LOGW(kTag, "failed to init namespace");
    return nullptr;
  }

  ESP_LOGI(kTag, "nvm storage initialised okay");
  return instance.release();
}

NvsStorage::NvsStorage(std::unique_ptr<tasks::Worker> worker,
                       nvs_handle_t handle)
    : writer_(std::move(worker)), handle_(handle) {}

NvsStorage::~NvsStorage() {
  nvs_close(handle_);
  nvs_flash_deinit();
}

auto NvsStorage::DowngradeSchemaSync() -> bool {
  ESP_LOGW(kTag, "namespace needs downgrading");
  return writer_
             ->Dispatch<bool>([&]() -> bool {
               nvs_erase_all(handle_);
               nvs_set_u8(handle_, kKeyVersion, kSchemaVersion);
               return nvs_commit(handle_);
             })
             .get() == ESP_OK;
}

auto NvsStorage::SchemaVersionSync() -> uint8_t {
  return writer_
      ->Dispatch<uint8_t>([&]() -> uint8_t {
        uint8_t ret;
        if (nvs_get_u8(handle_, kKeyVersion, &ret) != ESP_OK) {
          return UINT8_MAX;
        }
        return ret;
      })
      .get();
}

auto NvsStorage::PreferredBluetoothDevice()
    -> std::future<std::optional<bluetooth::mac_addr_t>> {
  return writer_->Dispatch<std::optional<bluetooth::mac_addr_t>>(
      [&]() -> std::optional<bluetooth::mac_addr_t> {
        bluetooth::mac_addr_t out{0};
        size_t size = out.size();
        if (nvs_get_blob(handle_, kKeyBluetooth, out.data(), &size) != ESP_OK) {
          return {};
        }
        return out;
      });
}

auto NvsStorage::PreferredBluetoothDevice(
    std::optional<bluetooth::mac_addr_t> addr) -> std::future<bool> {
  return writer_->Dispatch<bool>([&]() {
    if (!addr) {
      nvs_erase_key(handle_, kKeyBluetooth);
    } else {
      nvs_set_blob(handle_, kKeyBluetooth, addr.value().data(),
                   addr.value().size());
    }
    return nvs_commit(handle_) == ESP_OK;
  });
}

auto NvsStorage::OutputMode() -> std::future<Output> {
  return writer_->Dispatch<Output>([&]() -> Output {
    uint8_t out = 0;
    nvs_get_u8(handle_, kKeyOutput, &out);
    switch (out) {
      case static_cast<uint8_t>(Output::kBluetooth):
        return Output::kHeadphones;
      case static_cast<uint8_t>(Output::kHeadphones):
      default:
        return Output::kHeadphones;
    }
  });
}

auto NvsStorage::OutputMode(Output out) -> std::future<bool> {
  return writer_->Dispatch<bool>([&]() {
    nvs_set_u8(handle_, kKeyOutput, static_cast<uint8_t>(out));
    return nvs_commit(handle_) == ESP_OK;
  });
}

auto NvsStorage::ScreenBrightness() -> std::future<uint_fast8_t> {
  return writer_->Dispatch<uint_fast8_t>([&]() -> uint_fast8_t {
    uint8_t out = 50;
    nvs_get_u8(handle_, kKeyBrightness, &out);
    return out;
  });
}

auto NvsStorage::ScreenBrightness(uint_fast8_t val) -> std::future<bool> {
  return writer_->Dispatch<bool>([&]() {
    nvs_set_u8(handle_, kKeyBrightness, val);
    return nvs_commit(handle_) == ESP_OK;
  });
}

auto NvsStorage::AmpMaxVolume() -> std::future<uint16_t> {
  return writer_->Dispatch<uint16_t>([&]() -> uint16_t {
    uint16_t out = wm8523::kDefaultMaxVolume;
    nvs_get_u16(handle_, kKeyAmpMaxVolume, &out);
    return out;
  });
}

auto NvsStorage::AmpMaxVolume(uint16_t val) -> std::future<bool> {
  return writer_->Dispatch<bool>([&]() {
    nvs_set_u16(handle_, kKeyAmpMaxVolume, val);
    return nvs_commit(handle_) == ESP_OK;
  });
}

auto NvsStorage::AmpCurrentVolume() -> std::future<uint16_t> {
  return writer_->Dispatch<uint16_t>([&]() -> uint16_t {
    uint16_t out = wm8523::kDefaultVolume;
    nvs_get_u16(handle_, kKeyAmpCurrentVolume, &out);
    return out;
  });
}

auto NvsStorage::AmpCurrentVolume(uint16_t val) -> std::future<bool> {
  return writer_->Dispatch<bool>([&]() {
    nvs_set_u16(handle_, kKeyAmpCurrentVolume, val);
    return nvs_commit(handle_) == ESP_OK;
  });
}

}  // namespace drivers
