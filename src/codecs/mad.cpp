/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "mad.hpp"
#include <stdint.h>

#include <cstdint>
#include <optional>

#include "mad.h"

#include "codec.hpp"
#include "types.hpp"

namespace codecs {

static uint32_t scaleToBits(mad_fixed_t sample, uint8_t bits) {
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

auto MadMp3Decoder::GetOutputFormat() -> std::optional<OutputFormat> {
  if (synth_.pcm.channels == 0 || synth_.pcm.samplerate == 0) {
    return {};
  }
  return std::optional<OutputFormat>({
      .num_channels = static_cast<uint8_t>(synth_.pcm.channels),
      .bits_per_sample = 24,
      .sample_rate_hz = synth_.pcm.samplerate,
  });
}

auto MadMp3Decoder::SetInput(cpp::span<const std::byte> input) -> void {
  mad_stream_buffer(&stream_,
                    reinterpret_cast<const unsigned char*>(input.data()),
                    input.size());
}

auto MadMp3Decoder::GetInputPosition() -> std::size_t {
  return stream_.next_frame - stream_.buffer;
}

auto MadMp3Decoder::ProcessNextFrame() -> cpp::result<bool, ProcessingError> {
  // Whatever was last synthesized is now invalid, so ensure we don't try to
  // send it.
  current_sample_ = -1;

  // Decode the next frame. To signal errors, this returns -1 and
  // stashes an error code in the stream structure.
  if (mad_frame_decode(&frame_, &stream_) < 0) {
    if (MAD_RECOVERABLE(stream_.error)) {
      // Recoverable errors are usually malformed parts of the stream.
      // We can recover from them by just retrying the decode.
      return false;
    }

    if (stream_.error == MAD_ERROR_BUFLEN) {
      // The decoder ran out of bytes before it completed a frame. We
      // need to return back to the caller to give us more data.
      return true;
    }

    // The error is unrecoverable. Give up.
    return cpp::fail(MALFORMED_DATA);
  }

  // We've successfully decoded a frame!
  // Now we need to synthesize PCM samples based on the frame, and send
  // them downstream.
  mad_synth_frame(&synth_, &frame_);
  current_sample_ = 0;
  return false;
}

auto MadMp3Decoder::WriteOutputSamples(cpp::span<std::byte> output)
    -> std::pair<std::size_t, bool> {
  size_t output_byte = 0;
  // First ensure that we actually have some samples to send off.
  if (current_sample_ < 0) {
    return std::make_pair(output_byte, true);
  }

  while (current_sample_ < synth_.pcm.length) {
    if (output_byte + (2 * synth_.pcm.channels) >= output.size()) {
      return std::make_pair(output_byte, false);
    }

    for (int channel = 0; channel < synth_.pcm.channels; channel++) {
      uint32_t sample_24 =
          scaleToBits(synth_.pcm.samples[channel][current_sample_], 24);
      output[output_byte++] = static_cast<std::byte>((sample_24 >> 16) & 0xFF);
      output[output_byte++] = static_cast<std::byte>((sample_24 >> 8) & 0xFF);
      output[output_byte++] = static_cast<std::byte>((sample_24)&0xFF);
      // 24 bit samples must still be aligned to 32 bits. The LSB is ignored.
      output[output_byte++] = static_cast<std::byte>(0);
      /*
      uint16_t sample_16 =
          scaleToBits(synth_.pcm.samples[channel][current_sample_], 16);
      output[output_byte++] = static_cast<std::byte>((sample_16 >> 8) & 0xFF);
      output[output_byte++] = static_cast<std::byte>((sample_16)&0xFF);
      */
    }
    current_sample_++;
  }

  // We wrote everything! Reset, ready for the next frame.
  current_sample_ = -1;
  return std::make_pair(output_byte, true);
}

}  // namespace codecs
