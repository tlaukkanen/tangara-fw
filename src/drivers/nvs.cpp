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

[[maybe_unused]] static constexpr char kTag[] = "nvm";
static constexpr uint8_t kSchemaVersion = 1;

static constexpr char kKeyVersion[] = "ver";
static constexpr char kKeyBluetooth[] = "bt";
static constexpr char kKeyOutput[] = "out";
static constexpr char kKeyBrightness[] = "bright";
static constexpr char kKeyAmpMaxVolume[] = "hp_vol_max";
static constexpr char kKeyAmpCurrentVolume[] = "hp_vol";
static constexpr char kKeyAmpLeftBias[] = "hp_bias";
static constexpr char kKeyOnboarded[] = "intro";
static constexpr char kKeyPrimaryInput[] = "in_pri";
static constexpr char kKeyLockPolarity[] = "lockpol";

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

  std::unique_ptr<NvsStorage> instance = std::make_unique<NvsStorage>(handle);
  if (instance->SchemaVersionSync() < kSchemaVersion &&
      !instance->DowngradeSchemaSync()) {
    ESP_LOGW(kTag, "failed to init namespace");
    return nullptr;
  }

  ESP_LOGI(kTag, "nvm storage initialised okay");
  return instance.release();
}

NvsStorage::NvsStorage(nvs_handle_t handle) : handle_(handle) {}

NvsStorage::~NvsStorage() {
  nvs_close(handle_);
  nvs_flash_deinit();
}

auto NvsStorage::DowngradeSchemaSync() -> bool {
  ESP_LOGW(kTag, "namespace needs downgrading");
  nvs_erase_all(handle_);
  nvs_set_u8(handle_, kKeyVersion, kSchemaVersion);
  return nvs_commit(handle_);
}

auto NvsStorage::SchemaVersionSync() -> uint8_t {
  uint8_t ret;
  if (nvs_get_u8(handle_, kKeyVersion, &ret) != ESP_OK) {
    return UINT8_MAX;
  }
  return ret;
}

auto NvsStorage::LockPolarity() -> bool {
  uint8_t res;
  if (nvs_get_u8(handle_, kKeyLockPolarity, &res) != ESP_OK) {
    return false;
  }
  return res > 0;
}

auto NvsStorage::LockPolarity(bool p) -> bool {
  nvs_set_u8(handle_, kKeyLockPolarity, p);
  return nvs_commit(handle_) == ESP_OK;
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
    std::optional<bluetooth::mac_addr_t> addr) -> bool {
  if (!addr) {
    nvs_erase_key(handle_, kKeyBluetooth);
  } else {
    nvs_set_blob(handle_, kKeyBluetooth, addr.value().data(),
                 addr.value().size());
  }
  return nvs_commit(handle_) == ESP_OK;
}

auto NvsStorage::OutputMode() -> Output {
  uint8_t out = 0;
  nvs_get_u8(handle_, kKeyOutput, &out);
  switch (out) {
    case static_cast<uint8_t>(Output::kBluetooth):
      return Output::kBluetooth;
    case static_cast<uint8_t>(Output::kHeadphones):
    default:
      return Output::kHeadphones;
  }
}

auto NvsStorage::OutputMode(Output out) -> bool {
  uint8_t as_int = static_cast<uint8_t>(out);
  nvs_set_u8(handle_, kKeyOutput, as_int);
  return nvs_commit(handle_) == ESP_OK;
}

auto NvsStorage::ScreenBrightness() -> uint_fast8_t {
  uint8_t out = 50;
  nvs_get_u8(handle_, kKeyBrightness, &out);
  return out;
}

auto NvsStorage::ScreenBrightness(uint_fast8_t val) -> bool {
  nvs_set_u8(handle_, kKeyBrightness, val);
  return nvs_commit(handle_) == ESP_OK;
}

auto NvsStorage::AmpMaxVolume() -> uint16_t {
  uint16_t out = wm8523::kDefaultMaxVolume;
  nvs_get_u16(handle_, kKeyAmpMaxVolume, &out);
  return out;
}

auto NvsStorage::AmpMaxVolume(uint16_t val) -> bool {
  nvs_set_u16(handle_, kKeyAmpMaxVolume, val);
  return nvs_commit(handle_) == ESP_OK;
}

auto NvsStorage::AmpCurrentVolume() -> uint16_t {
  uint16_t out = wm8523::kDefaultVolume;
  nvs_get_u16(handle_, kKeyAmpCurrentVolume, &out);
  return out;
}

auto NvsStorage::AmpCurrentVolume(uint16_t val) -> bool {
  nvs_set_u16(handle_, kKeyAmpCurrentVolume, val);
  return nvs_commit(handle_) == ESP_OK;
}

auto NvsStorage::AmpLeftBias() -> int_fast8_t {
  int8_t out = 0;
  nvs_get_i8(handle_, kKeyAmpLeftBias, &out);
  return out;
}

auto NvsStorage::AmpLeftBias(int_fast8_t val) -> bool {
  nvs_set_i8(handle_, kKeyAmpLeftBias, val);
  return nvs_commit(handle_) == ESP_OK;
}

auto NvsStorage::HasShownOnboarding() -> bool {
  uint8_t out = false;
  nvs_get_u8(handle_, kKeyOnboarded, &out);
  return out;
}

auto NvsStorage::HasShownOnboarding(bool val) -> bool {
  nvs_set_u8(handle_, kKeyOnboarded, val);
  return nvs_commit(handle_) == ESP_OK;
}

auto NvsStorage::PrimaryInput() -> InputModes {
  uint8_t out = 3;
  nvs_get_u8(handle_, kKeyPrimaryInput, &out);
  switch (out) {
    case static_cast<uint8_t>(InputModes::kButtonsOnly):
      return InputModes::kButtonsOnly;
    case static_cast<uint8_t>(InputModes::kButtonsWithWheel):
      return InputModes::kButtonsWithWheel;
    case static_cast<uint8_t>(InputModes::kDirectionalWheel):
      return InputModes::kDirectionalWheel;
    case static_cast<uint8_t>(InputModes::kRotatingWheel):
      return InputModes::kRotatingWheel;
    default:
      return InputModes::kRotatingWheel;
  }
}

auto NvsStorage::PrimaryInput(InputModes mode) -> bool {
  uint8_t as_int = static_cast<uint8_t>(mode);
  nvs_set_u8(handle_, kKeyPrimaryInput, as_int);
  return nvs_commit(handle_) == ESP_OK;
}

}  // namespace drivers
