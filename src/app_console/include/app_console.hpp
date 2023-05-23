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
  explicit AppConsole(std::weak_ptr<database::Database> database);
  virtual ~AppConsole();

  std::weak_ptr<database::Database> database_;

 protected:
  virtual auto RegisterExtraComponents() -> void;
};

}  // namespace console
