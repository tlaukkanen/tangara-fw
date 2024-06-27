/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>

namespace console {

class Console {
 public:
  Console();
  virtual ~Console();

  auto Launch() -> void;
  virtual auto PrerunCallback() -> void;

 protected:
  virtual auto GetStackSizeKiB() -> uint16_t { return 8; }
  virtual auto RegisterExtraComponents() -> void {}

 private:
  auto RegisterCommonComponents() -> void;
};

}  // namespace console
