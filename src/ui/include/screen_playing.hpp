/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>

#include "lvgl.h"

#include "database.hpp"
#include "screen.hpp"

namespace ui {
namespace screens {

class Playing : public Screen {
 public:
  explicit Playing(database::Track t);
  ~Playing();

 private:
  database::Track track_;
};

}  // namespace screens
}  // namespace ui
