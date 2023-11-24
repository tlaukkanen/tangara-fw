/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <memory>
#include "codec.hpp"
#include "track.hpp"
#include "types.hpp"

namespace audio {

class TaggedStream : public codecs::IStream {
 public:
  TaggedStream(std::shared_ptr<database::TrackTags>,
               std::unique_ptr<codecs::IStream> wrapped);

  auto tags() -> std::shared_ptr<database::TrackTags>;

  auto Read(cpp::span<std::byte> dest) -> ssize_t override;

  auto CanSeek() -> bool override;

  auto SeekTo(int64_t destination, SeekFrom from) -> void override;

  auto CurrentPosition() -> int64_t override;

  auto SetPreambleFinished() -> void override;

 private:
  std::shared_ptr<database::TrackTags> tags_;
  std::unique_ptr<codecs::IStream> wrapped_;
};

class IAudioSource {
 public:
  virtual ~IAudioSource() {}

  virtual auto HasNewStream() -> bool = 0;
  virtual auto NextStream() -> std::shared_ptr<TaggedStream> = 0;
};

}  // namespace audio
