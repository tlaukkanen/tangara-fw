/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "freertos/FreeRTOS.h"

#include "ff.h"
#include "freertos/stream_buffer.h"

#include "audio/audio_source.hpp"
#include "codec.hpp"
#include "tasks.hpp"

namespace audio {

/*
 * Wraps another stream, proactively buffering large chunks of it into memory
 * at a time.
 */
class ReadaheadSource : public codecs::IStream {
 public:
  ReadaheadSource(tasks::WorkerPool&, std::unique_ptr<codecs::IStream>);
  ~ReadaheadSource();

  auto Read(std::span<std::byte> dest) -> ssize_t override;

  auto CanSeek() -> bool override;

  auto SeekTo(int64_t destination, SeekFrom from) -> void override;

  auto CurrentPosition() -> int64_t override;

  auto Size() -> std::optional<int64_t> override;

  auto SetPreambleFinished() -> void override;

  ReadaheadSource(const ReadaheadSource&) = delete;
  ReadaheadSource& operator=(const ReadaheadSource&) = delete;

 private:
  auto BeginReadahead() -> void;

  tasks::WorkerPool& worker_;
  std::unique_ptr<codecs::IStream> wrapped_;

  bool readahead_enabled_;
  std::atomic<bool> is_refilling_;
  StreamBufferHandle_t buffer_;
  int64_t tell_;
};

}  // namespace audio
