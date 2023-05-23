/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include "tinyfsm.hpp"

#include "song.hpp"

namespace audio {

struct PlaySong : tinyfsm::Event {
  database::SongId id;
};

}  // namespace audio
