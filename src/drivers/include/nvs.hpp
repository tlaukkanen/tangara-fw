/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "esp_err.h"
#include "nvs.h"

namespace drivers {

class NvsStorage {
 public:
  static auto Open() -> NvsStorage*;

  auto SchemaVersion() -> uint8_t;

  explicit NvsStorage(nvs_handle_t);
  ~NvsStorage();

 private:
  nvs_handle_t handle_;
};

}  // namespace drivers