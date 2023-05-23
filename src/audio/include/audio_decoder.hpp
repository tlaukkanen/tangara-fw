/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "chunk.hpp"
#include "ff.h"
#include "span.hpp"

#include "audio_element.hpp"
#include "codec.hpp"
#include "stream_info.hpp"

namespace audio {

/*
 * An audio element that accepts various kinds of encoded audio streams as
 * input, and converts them to uncompressed PCM output.
 */
class AudioDecoder : public IAudioElement {
 public:
  AudioDecoder();
  ~AudioDecoder();

  auto Process(const std::vector<InputStream>& inputs, OutputStream* output)
      -> void override;

  AudioDecoder(const AudioDecoder&) = delete;
  AudioDecoder& operator=(const AudioDecoder&) = delete;

 private:
  std::unique_ptr<codecs::ICodec> current_codec_;
  std::optional<StreamInfo::Format> current_input_format_;
  std::optional<StreamInfo::Format> current_output_format_;
  bool has_samples_to_send_;

  auto ProcessStreamInfo(const StreamInfo& info) -> bool;
};

}  // namespace audio
