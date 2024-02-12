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
#include "cppbor.h"
#include "cppbor_parse.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "tasks.hpp"
#include "wm8523.hpp"

namespace drivers {

[[maybe_unused]] static constexpr char kTag[] = "nvm";
static constexpr uint8_t kSchemaVersion = 1;

static constexpr char kKeyVersion[] = "ver";
static constexpr char kKeyBluetoothMac[] = "bt_mac";
static constexpr char kKeyBluetoothName[] = "bt_name";
static constexpr char kKeyBluetoothVolumes[] = "bt_vols";
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
    -> std::optional<bluetooth::MacAndName> {
  bluetooth::mac_addr_t mac{0};
  size_t size = mac.size();
  if (nvs_get_blob(handle_, kKeyBluetoothMac, mac.data(), &size) != ESP_OK) {
    return {};
  }
  size_t name_len = 0;
  if (nvs_get_str(handle_, kKeyBluetoothName, NULL, &name_len) != ESP_OK) {
  }
  char* raw_name = new char[name_len];
  if (nvs_get_str(handle_, kKeyBluetoothName, raw_name, &name_len) != ESP_OK) {
    delete[] raw_name;
    return {};
  }
  bluetooth::MacAndName out{
      .mac = mac,
      .name = {raw_name, name_len},
  };
  delete[] raw_name;
  return out;
}

auto NvsStorage::PreferredBluetoothDevice(
    std::optional<bluetooth::MacAndName> dev) -> bool {
  if (!dev) {
    nvs_erase_key(handle_, kKeyBluetoothMac);
    nvs_erase_key(handle_, kKeyBluetoothName);
  } else {
    nvs_set_blob(handle_, kKeyBluetoothMac, dev->mac.data(), dev->mac.size());
    nvs_set_str(handle_, kKeyBluetoothName, dev->name.c_str());
  }
  return nvs_commit(handle_) == ESP_OK;
}

class VolumesParseClient : public cppbor::ParseClient {
 public:
  VolumesParseClient(NvsStorage::BtVolumes& out)
      : state_(State::kInit), mac_(), vol_(), out_(out) {}

  ParseClient* item(std::unique_ptr<cppbor::Item>& item,
                    const uint8_t* hdrBegin,
                    const uint8_t* valueBegin,
                    const uint8_t* end) override {
    if (item->type() == cppbor::ARRAY) {
      if (state_ == State::kInit) {
        ESP_LOGI(kTag, "enter root");
        state_ = State::kRoot;
      } else if (state_ == State::kRoot) {
        ESP_LOGI(kTag, "enter pair");
        state_ = State::kPair;
      }
    } else if (item->type() == cppbor::BSTR && state_ == State::kPair) {
      ESP_LOGI(kTag, "get str");
      auto data = item->asBstr()->value();
      mac_.emplace();
      std::copy(data.begin(), data.end(), mac_->begin());
    } else if (item->type() == cppbor::UINT && state_ == State::kPair) {
      vol_ =
          std::clamp<uint64_t>(item->asUint()->unsignedValue(), 0, UINT8_MAX);
    }
    return this;
  }

  ParseClient* itemEnd(std::unique_ptr<cppbor::Item>& item,
                       const uint8_t* hdrBegin,
                       const uint8_t* valueBegin,
                       const uint8_t* end) override {
    if (item->type() == cppbor::ARRAY) {
      if (state_ == State::kRoot) {
        state_ = State::kFinished;
      } else if (state_ == State::kPair) {
        if (vol_ && mac_) {
          out_.Put(*mac_, *vol_);
        }
        mac_.reset();
        vol_.reset();
        state_ = State::kRoot;
      }
    }
    return this;
  }

  void error(const uint8_t* position,
             const std::string& errorMessage) override {}

 private:
  enum class State {
    kInit,
    kRoot,
    kPair,
    kFinished,
  };

  State state_;
  std::optional<bluetooth::mac_addr_t> mac_;
  std::optional<uint8_t> vol_;
  NvsStorage::BtVolumes& out_;
};

auto NvsStorage::BluetoothVolumes() -> BtVolumes {
  BtVolumes out;
  size_t encoded_len = 0;
  if (nvs_get_str(handle_, kKeyBluetoothVolumes, NULL, &encoded_len) !=
      ESP_OK) {
    return out;
  }
  auto encoded = std::unique_ptr<char[]>{new char[encoded_len]};
  if (nvs_get_str(handle_, kKeyBluetoothVolumes, encoded.get(), &encoded_len) !=
      ESP_OK) {
    return out;
  }
  VolumesParseClient client{out};
  auto data = reinterpret_cast<const uint8_t*>(encoded.get());
  cppbor::parse(data, data + encoded_len, &client);
  return out;
}

auto NvsStorage::BluetoothVolumes(const BtVolumes& vols) -> bool {
  cppbor::Array enc;
  auto vols_list = vols.Get();
  for (auto vol = vols_list.rbegin(); vol < vols_list.rend(); vol++) {
    enc.add(cppbor::Array{cppbor::Bstr{{vol->first.data(), vol->first.size()}},
                          cppbor::Uint{vol->second}});
  }
  std::string encoded = enc.toString();
  nvs_set_str(handle_, kKeyBluetoothVolumes, encoded.c_str());
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
