/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <cstddef>
#include <cstdint>
#include <future>
#include <memory>
#include <string>

#include "database/database.hpp"
#include "database/track.hpp"
#include "ff.h"
#include "freertos/portmacro.h"

#include "audio/audio_source.hpp"
#include "codec.hpp"
#include "database/future_fetcher.hpp"
#include "database/tag_parser.hpp"
#include "system_fsm/service_locator.hpp"
#include "tasks.hpp"
#include "types.hpp"

namespace audio {

/*
 * Utility to create streams that read from files on the sd card.
 */
class FatfsStreamFactory {
 public:
  explicit FatfsStreamFactory(system_fsm::ServiceLocator&);

  auto create(database::TrackId, uint32_t offset = 0)
      -> std::shared_ptr<TaggedStream>;
  auto create(std::string, uint32_t offset = 0)
      -> std::shared_ptr<TaggedStream>;

  FatfsStreamFactory(const FatfsStreamFactory&) = delete;
  FatfsStreamFactory& operator=(const FatfsStreamFactory&) = delete;

 private:
  auto ContainerToStreamType(database::Container)
      -> std::optional<codecs::StreamType>;

  system_fsm::ServiceLocator& services_;
};

}  // namespace audio