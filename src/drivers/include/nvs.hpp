/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <optional>

#include "esp_err.h"
#include "nvs.h"

#include "bluetooth_types.hpp"

namespace drivers {

class NvsStorage {
 public:
  static auto Open() -> NvsStorage*;

  auto SchemaVersion() -> uint8_t;

  auto PreferredBluetoothDevice() -> std::optional<bluetooth::mac_addr_t>;
  auto PreferredBluetoothDevice(std::optional<bluetooth::mac_addr_t>) -> void;

  explicit NvsStorage(nvs_handle_t);
  ~NvsStorage();

 private:
  nvs_handle_t handle_;
};

}  // namespace drivers