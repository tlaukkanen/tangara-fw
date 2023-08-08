/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <sys/_stdint.h>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "ogg/ogg.h"
#include "span.hpp"

namespace codecs {

class OggContainer {
 public:
  OggContainer();
  ~OggContainer();

  auto AddBytes(cpp::span<const std::byte>) -> void;
  auto HasNextPacket() -> bool;
  auto NextPacket() -> cpp::span<uint8_t>;
  auto PeekPacket() -> cpp::span<uint8_t>;

 private:
  ogg_sync_state sync_;
  ogg_stream_state stream_;
  ogg_page page_;
  ogg_packet packet_;
};

}  // namespace codecs