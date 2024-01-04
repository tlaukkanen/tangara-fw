/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#pragma once

#include <stdint.h>
#include <cstdint>
#include <optional>

namespace drivers {
namespace wm8523 {

extern const uint16_t kAbsoluteMaxVolume;

extern const uint16_t kAbsoluteMinVolume;

extern const uint16_t kMaxVolumeBeforeClipping;

extern const uint16_t kLineLevelReferenceVolume;

extern const uint16_t kDefaultVolume;
extern const uint16_t kDefaultMaxVolume;

extern const uint16_t kZeroDbVolume;

constexpr auto VolumeToDb(uint16_t vol) -> int_fast8_t {
  return (vol - kLineLevelReferenceVolume) / 4;
}

constexpr auto DbToVolume(int_fast8_t db) -> uint16_t {
  return (db * 4) + kLineLevelReferenceVolume;
}

enum class Register : uint8_t {
  kReset = 0,
  kRevision = 1,
  kPsCtrl = 2,
  kAifCtrl1 = 3,
  kAifCtrl2 = 4,
  kDacCtrl = 5,
  kDacGainLeft = 6,
  kDacGainRight = 7,
  kZeroDetect = 8,
};

auto ReadRegister(Register reg) -> std::optional<uint16_t>;
auto WriteRegister(Register reg, uint16_t data) -> bool;
auto WriteRegister(Register reg, uint8_t msb, uint8_t lsb) -> bool;

}  // namespace wm8523
}  // namespace drivers
