/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <memory>

#include "audio_converter.hpp"
#include "audio_events.hpp"
#include "audio_sink.hpp"
#include "audio_source.hpp"
#include "codec.hpp"
#include "track.hpp"
#include "types.hpp"

namespace audio {

/*
 * Handle to a persistent task that takes bytes from the given source, decodes
 * them into sample::Sample (normalised to 16 bit signed PCM), and then
 * forwards the resulting stream to the given converter.
 */
class Decoder {
 public:
  static auto Start(std::shared_ptr<IAudioSource> source,
                    std::shared_ptr<SampleConverter> converter) -> Decoder*;

  auto Main() -> void;

  Decoder(const Decoder&) = delete;
  Decoder& operator=(const Decoder&) = delete;

 private:
  Decoder(std::shared_ptr<IAudioSource> source,
          std::shared_ptr<SampleConverter> converter);

  auto BeginDecoding(std::shared_ptr<TaggedStream>) -> bool;
  auto ContinueDecoding() -> bool;

  std::shared_ptr<IAudioSource> source_;
  std::shared_ptr<SampleConverter> converter_;

  std::shared_ptr<codecs::IStream> stream_;
  std::unique_ptr<codecs::ICodec> codec_;

  std::optional<codecs::ICodec::OutputFormat> current_format_;
  std::optional<IAudioOutput::Format> current_sink_format_;

  std::span<sample::Sample> codec_buffer_;
};

}  // namespace audio
