/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio_source.hpp"
#include "codec.hpp"
#include "types.hpp"

namespace audio {

TaggedStream::TaggedStream(std::shared_ptr<database::TrackTags> t,
                           std::unique_ptr<codecs::IStream> w,
                           uint32_t offset)
    : codecs::IStream(w->type()), tags_(t), wrapped_(std::move(w)), offset_(offset) {}

auto TaggedStream::tags() -> std::shared_ptr<database::TrackTags> {
  return tags_;
}

auto TaggedStream::Read(cpp::span<std::byte> dest) -> ssize_t {
  return wrapped_->Read(dest);
}

auto TaggedStream::CanSeek() -> bool {
  return wrapped_->CanSeek();
}

auto TaggedStream::SeekTo(int64_t destination, SeekFrom from) -> void {
  wrapped_->SeekTo(destination, from);
}

auto TaggedStream::CurrentPosition() -> int64_t {
  return wrapped_->CurrentPosition();
}

auto TaggedStream::Size() -> std::optional<int64_t> {
  return wrapped_->Size();
}

auto TaggedStream::Offset() -> uint32_t {
  return offset_;
}

auto TaggedStream::SetPreambleFinished() -> void {
  wrapped_->SetPreambleFinished();
}

}  // namespace audio
