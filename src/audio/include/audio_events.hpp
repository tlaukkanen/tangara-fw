#pragma once

#include "tinyfsm.hpp"

#include "song.hpp"

namespace audio {

struct PlaySong : tinyfsm::Event {
  database::SongId id;
};

}  // namespace audio
