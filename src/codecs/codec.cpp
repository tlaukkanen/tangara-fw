/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "codec.hpp"

#include <memory>
#include <optional>

#include "dr_flac.hpp"
#include "mad.hpp"
#include "native.hpp"
#include "opus.hpp"
#include "types.hpp"
#include "vorbis.hpp"
#include "wav.hpp"

namespace codecs {

auto StreamTypeToString(StreamType t) -> std::string {
  switch (t) {
    case StreamType::kMp3:
      return "Mp3";
    case StreamType::kWav:
      return "Wav";
    case StreamType::kVorbis:
      return "Vorbis";
    case StreamType::kFlac:
      return "Flac";
    case StreamType::kOpus:
      return "Opus";
    case StreamType::kNative:
      return "Native";
    default:
      return "";
  }
}

auto CreateCodecForType(StreamType type) -> std::optional<ICodec*> {
  switch (type) {
    case StreamType::kMp3:
      return new MadMp3Decoder();
    case StreamType::kVorbis:
      return new TremorVorbisDecoder();
    case StreamType::kFlac:
      return new DrFlacDecoder();
    case StreamType::kOpus:
      return new XiphOpusDecoder();
    case StreamType::kWav:
      return new WavDecoder();
    case StreamType::kNative:
      return new NativeDecoder();
    default:
      return {};
  }
}

}  // namespace codecs
