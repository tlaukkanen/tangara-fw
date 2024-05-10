/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <memory>

#include "audio/audio_events.hpp"
#include "audio/audio_sink.hpp"
#include "audio/audio_source.hpp"
#include "audio/processor.hpp"
#include "codec.hpp"
#include "database/track.hpp"
#include "types.hpp"

namespace audio {

/*
 * Handle to a persistent task that takes encoded bytes from arbitrary sources,
 * decodes them into sample::Sample (normalised to 16 bit signed PCM), and then
 * streams them onward to the sample processor.
 */
class Decoder {
 public:
  static auto Start(std::shared_ptr<SampleProcessor>) -> Decoder*;

  auto open(std::shared_ptr<TaggedStream>) -> void;

  Decoder(const Decoder&) = delete;
  Decoder& operator=(const Decoder&) = delete;

 private:
  Decoder(std::shared_ptr<SampleProcessor>);

  auto Main() -> void;

  auto prepareDecode(std::shared_ptr<TaggedStream>) -> void;
  auto continueDecode() -> bool;
  auto finishDecode(bool cancel) -> void;

  std::shared_ptr<SampleProcessor> processor_;

  // Struct used with the next_stream_ queue.
  struct NextStream {
    std::shared_ptr<TaggedStream> stream;
  };
  QueueHandle_t next_stream_;

  std::shared_ptr<codecs::IStream> stream_;
  std::unique_ptr<codecs::ICodec> codec_;
  std::shared_ptr<TrackInfo> track_;

  std::span<sample::Sample> codec_buffer_;
};

}  // namespace audio
