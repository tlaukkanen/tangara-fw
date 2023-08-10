/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "mad.hpp"
#include <stdint.h>
#include <sys/_stdint.h>

#include <cstdint>
#include <cstring>
#include <optional>

#include "mad.h"

#include "codec.hpp"
#include "esp_log.h"
#include "result.hpp"
#include "sample.hpp"
#include "types.hpp"

namespace codecs {

static constexpr char kTag[] = "mad";

MadMp3Decoder::MadMp3Decoder()
    : input_(), buffer_(), current_sample_(-1), is_eof_(false), is_eos_(false) {
  mad_stream_init(&stream_);
  mad_frame_init(&frame_);
  mad_synth_init(&synth_);
}
MadMp3Decoder::~MadMp3Decoder() {
  mad_stream_finish(&stream_);
  mad_frame_finish(&frame_);
  mad_synth_finish(&synth_);
}

auto MadMp3Decoder::GetBytesUsed() -> std::size_t {
  if (stream_.next_frame) {
    return stream_.next_frame - stream_.buffer;
  } else {
    return stream_.bufend - stream_.buffer;
  }
}

auto MadMp3Decoder::OpenStream(std::shared_ptr<IStream> input)
    -> cpp::result<OutputFormat, ICodec::Error> {
  input_ = input;

  // To get the output format for MP3 streams, we simply need to decode the
  // first frame header.
  mad_header header;
  mad_header_init(&header);
  bool eof = false;
  bool got_header = false;
  while (!eof && !got_header) {
    eof = buffer_.Refill(input_.get());

    buffer_.ConsumeBytes([&](cpp::span<std::byte> buf) -> size_t {
      mad_stream_buffer(&stream_,
                        reinterpret_cast<const unsigned char*>(buf.data()),
                        buf.size_bytes());

      while (mad_header_decode(&header, &stream_) < 0) {
        if (MAD_RECOVERABLE(stream_.error)) {
          // Recoverable errors are usually malformed parts of the stream.
          // We can recover from them by just retrying the decode.
          continue;
        }
        if (stream_.error == MAD_ERROR_BUFLEN) {
          return GetBytesUsed();
        }
        eof = true;
        return 0;
      }

      got_header = true;
      return GetBytesUsed();
    });
  }

  if (!got_header) {
    return cpp::fail(ICodec::Error::kMalformedData);
  }

  uint8_t channels = MAD_NCHANNELS(&header);
  OutputFormat output{
      .num_channels = channels,
      .sample_rate_hz = header.samplerate,
  };

  auto vbr_length = GetVbrLength(header);
  if (vbr_length) {
    output.duration_seconds = vbr_length;
  }
  return output;
}

auto MadMp3Decoder::DecodeTo(cpp::span<sample::Sample> output)
    -> cpp::result<OutputInfo, Error> {
  if (current_sample_ < 0 && !is_eos_) {
    if (!is_eof_) {
      is_eof_ = buffer_.Refill(input_.get());
      if (is_eof_) {
        buffer_.AddBytes([&](cpp::span<std::byte> buf) -> size_t {
          if (buf.size() < 8) {
            is_eof_ = false;
            return 0;
          }
          ESP_LOGI(kTag, "adding MAD_HEADER_GUARD");
          std::fill_n(buf.begin(), 8, std::byte(0));
          return 8;
        });
      }
    }

    buffer_.ConsumeBytes([&](cpp::span<std::byte> buf) -> size_t {
      mad_stream_buffer(&stream_,
                        reinterpret_cast<const unsigned char*>(buf.data()),
                        buf.size());

      // Decode the next frame. To signal errors, this returns -1 and
      // stashes an error code in the stream structure.
      while (mad_frame_decode(&frame_, &stream_) < 0) {
        if (MAD_RECOVERABLE(stream_.error)) {
          // Recoverable errors are usually malformed parts of the stream.
          // We can recover from them by just retrying the decode.
          continue;
        }
        if (stream_.error == MAD_ERROR_BUFLEN) {
          if (is_eof_) {
            ESP_LOGI(kTag, "BUFLEN while eof; this is eos");
            is_eos_ = true;
          }
          return GetBytesUsed();
        }
        // The error is unrecoverable. Give up.
        is_eof_ = true;
        is_eos_ = true;
        return 0;
      }

      // We've successfully decoded a frame! Now synthesize samples to write
      // out.
      mad_synth_frame(&synth_, &frame_);
      current_sample_ = 0;
      return GetBytesUsed();
    });
  }

  size_t output_sample = 0;
  if (current_sample_ >= 0) {
    while (current_sample_ < synth_.pcm.length) {
      if (output_sample + synth_.pcm.channels >= output.size()) {
        // We can't fit the next full frame into the buffer.
        return OutputInfo{.samples_written = output_sample,
                          .is_stream_finished = false};
      }

      for (int channel = 0; channel < synth_.pcm.channels; channel++) {
        output[output_sample++] =
            sample::FromMad(synth_.pcm.samples[channel][current_sample_]);
      }
      current_sample_++;
    }
  }

  // We wrote everything! Reset, ready for the next frame.
  current_sample_ = -1;
  return OutputInfo{.samples_written = output_sample,
                    .is_stream_finished = is_eos_};
}

auto MadMp3Decoder::SeekTo(std::size_t target_sample)
    -> cpp::result<void, Error> {
  return {};
}

/*
 * Implementation taken from SDL_mixer and modified. Original is zlib-licensed,
 * copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>
 */
auto MadMp3Decoder::GetVbrLength(const mad_header& header)
    -> std::optional<uint32_t> {
  if (!stream_.this_frame || !stream_.next_frame ||
      stream_.next_frame <= stream_.this_frame ||
      (stream_.next_frame - stream_.this_frame) < 48) {
    return {};
  }

  int mpeg_version = (stream_.this_frame[1] >> 3) & 0x03;

  int xing_offset = 0;
  switch (mpeg_version) {
    case 0x03: /* MPEG1 */
      if (header.mode == MAD_MODE_SINGLE_CHANNEL) {
        xing_offset = 4 + 17;
      } else {
        xing_offset = 4 + 32;
      }
      break;
    default: /* MPEG2 and MPEG2.5 */
      if (header.mode == MAD_MODE_SINGLE_CHANNEL) {
        xing_offset = 4 + 17;
      } else {
        xing_offset = 4 + 9;
      }
      break;
  }

  uint32_t samples_per_frame = 32 * MAD_NSBSAMPLES(&header);

  unsigned char const* frames_count_raw;
  uint32_t frames_count = 0;
  // TODO(jacqueline): we should also look up any toc fields here, to make
  // seeking faster.
  if (std::memcmp(stream_.this_frame + xing_offset, "Xing", 4) == 0 ||
      std::memcmp(stream_.this_frame + xing_offset, "Info", 4) == 0) {
    /* Xing header to get the count of frames for VBR */
    frames_count_raw = stream_.this_frame + xing_offset + 8;
    frames_count = ((uint32_t)frames_count_raw[0] << 24) +
                   ((uint32_t)frames_count_raw[1] << 16) +
                   ((uint32_t)frames_count_raw[2] << 8) +
                   ((uint32_t)frames_count_raw[3]);
  } else if (std::memcmp(stream_.this_frame + xing_offset, "VBRI", 4) == 0) {
    /* VBRI header to get the count of frames for VBR */
    frames_count_raw = stream_.this_frame + xing_offset + 14;
    frames_count = ((uint32_t)frames_count_raw[0] << 24) +
                   ((uint32_t)frames_count_raw[1] << 16) +
                   ((uint32_t)frames_count_raw[2] << 8) +
                   ((uint32_t)frames_count_raw[3]);
  } else {
    return {};
  }

  return (double)(frames_count * samples_per_frame) / header.samplerate;
}

}  // namespace codecs
