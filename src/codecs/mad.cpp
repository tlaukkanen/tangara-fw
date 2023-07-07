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
#include "types.hpp"

namespace codecs {

static uint32_t mad_fixed_to_pcm(mad_fixed_t sample, uint8_t bits) {
  // Round the bottom bits.
  sample += (1L << (MAD_F_FRACBITS - bits));

  // Clip the leftover bits to within range.
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  // Quantize.
  return sample >> (MAD_F_FRACBITS + 1 - bits);
}

MadMp3Decoder::MadMp3Decoder() {
  mad_stream_init(&stream_);
  mad_frame_init(&frame_);
  mad_synth_init(&synth_);
}
MadMp3Decoder::~MadMp3Decoder() {
  mad_stream_finish(&stream_);
  mad_frame_finish(&frame_);
  mad_synth_finish(&synth_);
}

auto MadMp3Decoder::GetBytesUsed(std::size_t buffer_size) -> std::size_t {
  if (stream_.next_frame) {
    std::size_t remaining = stream_.bufend - stream_.next_frame;
    return buffer_size - remaining;
  } else {
    return stream_.bufend - stream_.buffer;
  }
}

auto MadMp3Decoder::BeginStream(const cpp::span<const std::byte> input)
    -> Result<OutputFormat> {
  mad_stream_buffer(&stream_,
                    reinterpret_cast<const unsigned char*>(input.data()),
                    input.size_bytes());
  // Whatever was last synthesized is now invalid, so ensure we don't try to
  // send it.
  current_sample_ = -1;

  // To get the output format for MP3 streams, we simply need to decode the
  // first frame header.
  mad_header header;
  mad_header_init(&header);
  while (mad_header_decode(&header, &stream_) < 0) {
    if (MAD_RECOVERABLE(stream_.error)) {
      // Recoverable errors are usually malformed parts of the stream.
      // We can recover from them by just retrying the decode.
      continue;
    }
    if (stream_.error == MAD_ERROR_BUFLEN) {
      return {GetBytesUsed(input.size_bytes()), cpp::fail(Error::kOutOfInput)};
    }
    return {GetBytesUsed(input.size_bytes()), cpp::fail(Error::kMalformedData)};
  }

  uint8_t channels = MAD_NCHANNELS(&header);
  OutputFormat output{
      .num_channels = channels,
      .bits_per_sample = 24,  // We always scale to 24 bits
      .sample_rate_hz = header.samplerate,
      .duration_seconds = {},
      .bits_per_second = {},
  };

  auto vbr_length = GetVbrLength(header);
  if (vbr_length) {
    output.duration_seconds = vbr_length;
  } else {
    output.bits_per_second = header.bitrate;
  }

  return {GetBytesUsed(input.size_bytes()), output};
}

auto MadMp3Decoder::ContinueStream(cpp::span<const std::byte> input,
                                   cpp::span<std::byte> output)
    -> Result<OutputInfo> {
  std::size_t bytes_read = 0;
  if (current_sample_ < 0) {
    mad_stream_buffer(&stream_,
                      reinterpret_cast<const unsigned char*>(input.data()),
                      input.size());

    // Decode the next frame. To signal errors, this returns -1 and
    // stashes an error code in the stream structure.
    while (mad_frame_decode(&frame_, &stream_) < 0) {
      if (MAD_RECOVERABLE(stream_.error)) {
        // Recoverable errors are usually malformed parts of the stream.
        // We can recover from them by just retrying the decode.
        continue;
      }
      if (stream_.error == MAD_ERROR_BUFLEN) {
        // The decoder ran out of bytes before it completed a frame. We
        // need to return back to the caller to give us more data.
        return {GetBytesUsed(input.size_bytes()),
                cpp::fail(Error::kOutOfInput)};
      }
      // The error is unrecoverable. Give up.
      return {GetBytesUsed(input.size_bytes()),
              cpp::fail(Error::kMalformedData)};
    }

    // We've successfully decoded a frame! Now synthesize samples to write out.
    mad_synth_frame(&synth_, &frame_);
    current_sample_ = 0;
    bytes_read = GetBytesUsed(input.size_bytes());
  }

  size_t output_byte = 0;
  while (current_sample_ < synth_.pcm.length) {
    if (output_byte + (4 * synth_.pcm.channels) >= output.size()) {
      // We can't fit the next sample into the buffer. Stop now, and also avoid
      // writing the sample for only half the channels.
      return {bytes_read, OutputInfo{.bytes_written = output_byte,
                                     .is_finished_writing = false}};
    }

    for (int channel = 0; channel < synth_.pcm.channels; channel++) {
      uint32_t sample_24 =
          mad_fixed_to_pcm(synth_.pcm.samples[channel][current_sample_], 24);
      output[output_byte++] = static_cast<std::byte>((sample_24 >> 16) & 0xFF);
      output[output_byte++] = static_cast<std::byte>((sample_24 >> 8) & 0xFF);
      output[output_byte++] = static_cast<std::byte>((sample_24)&0xFF);
      // 24 bit samples must still be aligned to 32 bits. The LSB is ignored.
      output[output_byte++] = static_cast<std::byte>(0);
    }
    current_sample_++;
  }

  // We wrote everything! Reset, ready for the next frame.
  current_sample_ = -1;
  return {bytes_read, OutputInfo{.bytes_written = output_byte,
                                 .is_finished_writing = true}};
}

auto MadMp3Decoder::SeekStream(cpp::span<const std::byte> input,
                               std::size_t target_sample) -> Result<void> {
  mad_stream_buffer(&stream_,
                    reinterpret_cast<const unsigned char*>(input.data()),
                    input.size());
  std::size_t current_sample = 0;
  std::size_t samples_per_frame = 0;
  while (true) {
    current_sample += samples_per_frame;

    // First, decode the header for this frame.
    mad_header header;
    mad_header_init(&header);
    while (mad_header_decode(&header, &stream_) < 0) {
      if (MAD_RECOVERABLE(stream_.error)) {
        // Recoverable errors are usually malformed parts of the stream.
        // We can recover from them by just retrying the decode.
        continue;
      } else {
        // Don't bother checking for other errors; if the first part of the
        // stream doesn't even contain a header then something's gone wrong.
        return {GetBytesUsed(input.size_bytes()),
                cpp::fail(Error::kMalformedData)};
      }
    }

    // Calculate samples per frame if we haven't already.
    if (samples_per_frame == 0) {
      samples_per_frame = 32 * MAD_NSBSAMPLES(&header);
    }

    // Work out how close we are to the target.
    std::size_t samples_to_go = target_sample - current_sample;
    std::size_t frames_to_go = samples_to_go / samples_per_frame;
    if (frames_to_go > 3) {
      // The target is far in the distance. Keep skipping through headers only.
      continue;
    }

    // The target is within the next few frames. We should decode these, to give
    // the decoder a chance to sync with the stream.
    while (mad_frame_decode(&frame_, &stream_) < 0) {
      if (MAD_RECOVERABLE(stream_.error)) {
        continue;
      }
      if (stream_.error == MAD_ERROR_BUFLEN) {
        return {GetBytesUsed(input.size_bytes()),
                cpp::fail(Error::kOutOfInput)};
      }
      // The error is unrecoverable. Give up.
      return {GetBytesUsed(input.size_bytes()),
              cpp::fail(Error::kMalformedData)};
    }

    if (frames_to_go <= 1) {
      // The target is within the next couple of frames. We should start
      // synthesizing a frame early because this guy says so:
      // https://lists.mars.org/hyperkitty/list/mad-dev@lists.mars.org/message/UZSHXZTIZEF7FZ4KFOR65DUCKAY2OCUT/
      mad_synth_frame(&synth_, &frame_);
    }

    if (frames_to_go == 0) {
      // The target is actually within this frame! Set up for the ContinueStream
      // call.
      current_sample_ =
          (target_sample > current_sample) ? target_sample - current_sample : 0;
      return {GetBytesUsed(input.size_bytes()), {}};
    }
  }
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
