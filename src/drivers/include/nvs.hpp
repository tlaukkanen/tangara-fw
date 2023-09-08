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

  auto PreferredBluetoothDevice()
      -> std::future<std::optional<bluetooth::mac_addr_t>>;
  auto PreferredBluetoothDevice(std::optional<bluetooth::mac_addr_t>)
      -> std::future<bool>;

  enum class Output : uint8_t {
    kHeadphones = 0,
    kBluetooth = 1,
  };
  auto OutputMode() -> std::future<Output>;
  auto OutputMode(Output) -> std::future<bool>;

  auto ScreenBrightness() -> std::future<uint_fast8_t>;
  auto ScreenBrightness(uint_fast8_t) -> std::future<bool>;

  auto AmpMaxVolume() -> std::future<uint16_t>;
  auto AmpMaxVolume(uint16_t) -> std::future<bool>;

  auto AmpCurrentVolume() -> std::future<uint16_t>;
  auto AmpCurrentVolume(uint16_t) -> std::future<bool>;

  auto HasShownOnboarding() -> std::future<bool>;
  auto HasShownOnboarding(bool) -> std::future<bool>;

  explicit NvsStorage(std::unique_ptr<tasks::Worker>, nvs_handle_t);
  ~NvsStorage();

 private:
  auto DowngradeSchemaSync() -> bool;
  auto SchemaVersionSync() -> uint8_t;

  std::unique_ptr<tasks::Worker> writer_;
  nvs_handle_t handle_;
};

}  // namespace drivers