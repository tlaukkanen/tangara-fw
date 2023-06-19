/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "console.hpp"
#include "database.hpp"

namespace console {

class AppConsole : public Console {
 public:
  static std::weak_ptr<database::Database> sDatabase;

 protected:
  virtual auto RegisterExtraComponents() -> void;
};

}  // namespace console
