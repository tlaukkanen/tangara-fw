/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "esp_partition.h"

#include "strxfrm.h"

namespace locale {

/*
 * Interface for sorting database entries.
 * For performance, our database exclusively orders entries via byte
 * comparisons of each key. Our collators therefore work by transforming keys
 * such that a byte-order comparison results in a natural ordering.
 */
class ICollator {
 public:
  virtual ~ICollator() {}

  /*
   * Returns an identify that uniquely describes this collator. Does not need
   * to be human readable.
   */
  virtual auto Describe() -> std::optional<std::string> = 0;
  virtual auto Transform(const std::string&) -> std::string = 0;
};

/* Creates and returns the best available collator. */
auto CreateCollator() -> std::unique_ptr<ICollator>;

/*
 * Collator that doesn't do anything. Used only when there is no available
 * locale data.
 */
class NoopCollator : public ICollator {
 public:
  auto Describe() -> std::optional<std::string> override { return {}; }
  auto Transform(const std::string& in) -> std::string override { return in; }
};

/*
 * Collator that uses glibc's `strxfrm` to transform keys. Relies on an
 * LC_COLLATE file (+ 8 byte name header) flashed on a partition in internal
 * flash.
 */
class GLibCollator : public ICollator {
 public:
  static auto create() -> GLibCollator*;
  ~GLibCollator();

  auto Describe() -> std::optional<std::string> override { return name_; }
  auto Transform(const std::string& in) -> std::string override;

 private:
  GLibCollator(const std::string& name,
               const esp_partition_mmap_handle_t,
               std::unique_ptr<locale_data_t>);

  const std::string name_;
  const esp_partition_mmap_handle_t handle_;
  std::unique_ptr<locale_data_t> locale_data_;
};

}  // namespace locale
