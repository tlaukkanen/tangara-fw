/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ogg.hpp"
#include <cstring>

#include "esp_log.h"
#include "ogg/ogg.h"

namespace codecs {

static constexpr char kTag[] = "ogg";

OggContainer::OggContainer()
    : sync_(),
      stream_(),
      page_(),
      packet_(),
      has_stream_(false),
      has_packet_(false) {
  ogg_sync_init(&sync_);
  ogg_sync_pageout(&sync_, &page_);
}

OggContainer::~OggContainer() {
  ogg_sync_clear(&sync_);
  if (has_stream_) {
    ogg_stream_clear(&stream_);
  }
}

auto OggContainer::AddBytes(cpp::span<const std::byte> in) -> bool {
  ESP_LOGI(kTag, "adding %u bytes to buffer", in.size());
  char* buf = ogg_sync_buffer(&sync_, in.size());
  if (buf == NULL) {
    ESP_LOGE(kTag, "failed to allocate sync buffer");
    return false;
  }
  std::memcpy(buf, in.data(), in.size());
  if (ogg_sync_wrote(&sync_, in.size()) < 0) {
    ESP_LOGE(kTag, "failed to write to sync buffer");
    return false;
  }
  return AdvancePage() && AdvancePacket();
}

auto OggContainer::HasPacket() -> bool {
  return has_packet_;
}

auto OggContainer::Next() -> bool {
  if (AdvancePacket()) {
    return true;
  }
  if (AdvancePage() && AdvancePacket()) {
    return true;
  }
  return false;
}

auto OggContainer::Current() -> cpp::span<uint8_t> {
  if (!has_packet_) {
    return {};
  }
  ESP_LOGI(kTag, "getting packet, location %p size %li", packet_.packet,
           packet_.bytes);
  return {packet_.packet, static_cast<size_t>(packet_.bytes)};
}

auto OggContainer::AdvancePage() -> bool {
  int err;
  if ((err = ogg_sync_pageout(&sync_, &page_)) != 1) {
    ESP_LOGE(kTag, "failed to assemble page, res %i", err);
    return false;
  }
  if (!has_stream_) {
    int serialno = ogg_page_serialno(&page_);
    ESP_LOGI(kTag, "beginning ogg stream, serial number %i", serialno);
    if ((err = ogg_stream_init(&stream_, serialno) < 0)) {
      ESP_LOGE(kTag, "failed to init stream page, res %i", err);
      return false;
    }
    has_stream_ = true;
  }
  if (ogg_stream_pagein(&stream_, &page_) < 0) {
    ESP_LOGE(kTag, "failed to read in page");
    return false;
  }
  return true;
}

auto OggContainer::AdvancePacket() -> bool {
  has_packet_ = false;
  int res;
  while ((res = ogg_stream_packetout(&stream_, &packet_)) == -1) {
    // Retry until we sync, or run out of data.
    ESP_LOGW(kTag, "trying to sync stream...");
  }
  has_packet_ = res;
  if (!has_packet_) {
    ESP_LOGE(kTag, "failed to read out packet");
  }
  return has_packet_;
}

}  // namespace codecs