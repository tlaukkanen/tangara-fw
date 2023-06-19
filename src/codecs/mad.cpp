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

auto MadMp3Decoder::GetInputPosition() -> std::size_t {
  return stream_.next_frame - stream_.buffer;
}

auto MadMp3Decoder::BeginStream(const cpp::span<const std::byte> input)
    -> Result<OutputFormat> {
  mad_stream_buffer(&stream_,
                    reinterpret_cast<const unsigned char*>(input.data()),
                    input.size());
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
    } else {
      // Don't bother checking for other errors; if the first part of the stream
      // doesn't even contain a header then something's gone wrong.
      return {GetInputPosition(), cpp::fail(Error::kMalformedData)};
    }
  }

  uint8_t channels = MAD_NCHANNELS(&header);
  return {GetInputPosition(),
          OutputFormat{
              .num_channels = channels,
              .bits_per_sample = 24,  // We always scale to 24 bits
              .sample_rate_hz = header.samplerate,
          }};
}

auto MadMp3Decoder::ContinueStream(cpp::span<const std::byte> input,
                                   cpp::span<std::byte> output)
    -> Result<OutputInfo> {
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
        return {GetInputPosition(), cpp::fail(Error::kOutOfInput)};
      }
      // The error is unrecoverable. Give up.
      return {GetInputPosition(), cpp::fail(Error::kMalformedData)};
    }

    // We've successfully decoded a frame! Now synthesize samples to write out.
    mad_synth_frame(&synth_, &frame_);
    current_sample_ = 0;
  }

  size_t output_byte = 0;
  while (current_sample_ < synth_.pcm.length) {
    if (output_byte + (4 * synth_.pcm.channels) >= output.size()) {
      // We can't fit the next sample into the buffer. Stop now, and also avoid
      // writing the sample for only half the channels.
      return {GetInputPosition(), OutputInfo{.bytes_written = output_byte,
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
  return {GetInputPosition(), OutputInfo{.bytes_written = output_byte,
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
        return {GetInputPosition(), cpp::fail(Error::kMalformedData)};
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
        return {GetInputPosition(), cpp::fail(Error::kOutOfInput)};
      }
      // The error is unrecoverable. Give up.
      return {GetInputPosition(), cpp::fail(Error::kMalformedData)};
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
      return {GetInputPosition(), {}};
    }
  }
}

}  // namespace codecs
