/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <memory>
#include <optional>

#include "esp_err.h"
#include "nvs.h"

#include "bluetooth_types.hpp"
#include "tasks.hpp"

namespace drivers {

class NvsStorage {
 public:
  static auto OpenSync() -> NvsStorage*;

  auto PreferredBluetoothDevice() -> std::optional<bluetooth::mac_addr_t>;
  auto PreferredBluetoothDevice(std::optional<bluetooth::mac_addr_t>) -> bool;

  enum class Output : uint8_t {
    kHeadphones = 0,
    kBluetooth = 1,
  };
  auto OutputMode() -> Output;
  auto OutputMode(Output) -> bool;

  auto ScreenBrightness() -> uint_fast8_t;
  auto ScreenBrightness(uint_fast8_t) -> bool;

  auto AmpMaxVolume() -> uint16_t;
  auto AmpMaxVolume(uint16_t) -> bool;

  auto AmpCurrentVolume() -> uint16_t;
  auto AmpCurrentVolume(uint16_t) -> bool;

  auto HasShownOnboarding() -> bool;
  auto HasShownOnboarding(bool) -> bool;

  explicit NvsStorage(nvs_handle_t);
  ~NvsStorage();

 private:
  auto DowngradeSchemaSync() -> bool;
  auto SchemaVersionSync() -> uint8_t;

  nvs_handle_t handle_;
};

}  // namespace drivers