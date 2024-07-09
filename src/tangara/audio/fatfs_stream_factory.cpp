/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio/fatfs_stream_factory.hpp"

#include <cstdint>
#include <memory>
#include <string>

#include "esp_log.h"
#include "ff.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"

#include "audio/audio_source.hpp"
#include "audio/fatfs_source.hpp"
#include "codec.hpp"
#include "database/database.hpp"
#include "database/tag_parser.hpp"
#include "database/track.hpp"
#include "drivers/spi.hpp"
#include "tasks.hpp"
#include "types.hpp"

[[maybe_unused]] static const char* kTag = "SRC";

namespace audio {

FatfsStreamFactory::FatfsStreamFactory(database::Handle&& handle,
                                       database::ITagParser& parser)
    : db_(handle), tag_parser_(parser) {}

auto FatfsStreamFactory::create(database::TrackId id, uint32_t offset)
    -> std::shared_ptr<TaggedStream> {
  auto db = db_.lock();
  if (!db) {
    return {};
  }
  auto path = db->getTrackPath(id);
  if (!path) {
    return {};
  }
  return create(*path, offset);
}

auto FatfsStreamFactory::create(std::string path, uint32_t offset)
    -> std::shared_ptr<TaggedStream> {
  auto tags = tag_parser_.ReadAndParseTags(path);
  if (!tags) {
    ESP_LOGE(kTag, "failed to read tags");
    return {};
  }

  if (!tags->title()) {
    tags->title(path);
  }

  auto stream_type = ContainerToStreamType(tags->encoding());
  if (!stream_type.has_value()) {
    ESP_LOGE(kTag, "couldn't match container to stream");
    return {};
  }

  std::unique_ptr<FIL> file = std::make_unique<FIL>();
  FRESULT res = f_open(file.get(), path.c_str(), FA_READ);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "failed to open file! res: %i", res);
    return {};
  }

  return std::make_shared<TaggedStream>(
      tags, std::make_unique<FatfsSource>(stream_type.value(), std::move(file)),
      path, offset);
}

auto FatfsStreamFactory::ContainerToStreamType(database::Container enc)
    -> std::optional<codecs::StreamType> {
  switch (enc) {
    case database::Container::kMp3:
      return codecs::StreamType::kMp3;
    case database::Container::kWav:
      return codecs::StreamType::kWav;
    case database::Container::kOgg:
      return codecs::StreamType::kVorbis;
    case database::Container::kFlac:
      return codecs::StreamType::kFlac;
    case database::Container::kOpus:
      return codecs::StreamType::kOpus;
    case database::Container::kUnsupported:
    default:
      return {};
  }
}

}  // namespace audio
