/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "esp_partition.h"
#include "span.hpp"

#include "strxfrm.h"

namespace locale {

class ICollator {
 public:
  virtual ~ICollator() {}

  virtual auto Transform(const std::string&) -> std::string = 0;
};

class NoopCollator : public ICollator {
 public:
  auto Transform(const std::string& in) -> std::string override { return in; }
};

auto CreateCollator() -> std::unique_ptr<ICollator>;

class GLibCollator : public ICollator {
 public:
  static auto create() -> GLibCollator*;
  ~GLibCollator();

  auto Transform(const std::string& in) -> std::string override;

 private:
  GLibCollator(const esp_partition_mmap_handle_t,
               std::unique_ptr<locale_data_t>);

  const esp_partition_mmap_handle_t handle_;
  std::unique_ptr<locale_data_t> locale_data_;
};

}  // namespace locale
