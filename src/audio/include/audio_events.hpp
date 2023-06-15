/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <string>

#include "tinyfsm.hpp"

#include "track.hpp"

namespace audio {

struct PlayFile : tinyfsm::Event {
  std::string filename;
};

struct PlayTrack : tinyfsm::Event {
  database::TrackId id;
  std::optional<database::TrackData> data;
  std::optional<database::TrackTags> tags;
};

struct InputFileFinished : tinyfsm::Event {};
struct AudioPipelineIdle : tinyfsm::Event {};

}  // namespace audio
