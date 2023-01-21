#include "mad.hpp"

#include <cstdint>

#include "mad.h"

#include "codec.hpp"

namespace codecs {

static int32_t scaleTo24Bits(mad_fixed_t sample) {
  // Round the bottom bits.
  sample += (1L << (MAD_F_FRACBITS - 24));

  // Clip the leftover bits to within range.
  if (sample >= MAD_F_ONE)
    sample = MAD_F_ONE - 1;
  else if (sample < -MAD_F_ONE)
    sample = -MAD_F_ONE;

  /* quantize */
  return sample >> (MAD_F_FRACBITS + 1 - 24);
}

MadMp3Decoder::MadMp3Decoder() {
  mad_stream_init(&stream_);
  mad_frame_init(&frame_);
  mad_synth_init(&synth_);
  mad_header_init(&header_);
}
MadMp3Decoder::~MadMp3Decoder() {
  mad_stream_finish(&stream_);
  mad_frame_finish(&frame_);
  mad_synth_finish(&synth_);
  mad_header_finish(&header_);
}

auto MadMp3Decoder::CanHandleFile(const std::string& path) -> bool {
  return true;  // TODO.
}

auto MadMp3Decoder::GetOutputFormat() -> OutputFormat {
  return OutputFormat{
      .num_channels = static_cast<uint8_t>(synth_.pcm.channels),
      .bits_per_sample = 24,
      .sample_rate_hz = synth_.pcm.samplerate,
  };
}

auto MadMp3Decoder::ResetForNewStream() -> void {
  has_decoded_header_ = false;
}

auto MadMp3Decoder::SetInput(cpp::span<std::byte> input) -> void {
  mad_stream_buffer(&stream_,
                    reinterpret_cast<const unsigned char*>(input.data()),
                    input.size());
}

auto MadMp3Decoder::GetInputPosition() -> std::size_t {
  return stream_.next_frame - stream_.buffer;
}

auto MadMp3Decoder::ProcessNextFrame() -> cpp::result<bool, ProcessingError> {
  if (!has_decoded_header_) {
    // The header of any given frame should be representative of the
    // entire stream, so only need to read it once.
    mad_header_decode(&header_, &stream_);
    has_decoded_header_ = true;

    // TODO: Use the info in the header for something. I think the
    // duration will help with seeking?
  }

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
    if (output_byte + (3 * synth_.pcm.channels) >= output.size()) {
      return std::make_pair(output_byte, false);
    }

    for (int channel = 0; channel < synth_.pcm.channels; channel++) {
      uint32_t sample_24 =
          scaleTo24Bits(synth_.pcm.samples[channel][current_sample_]);
      output[output_byte++] = static_cast<std::byte>(sample_24 >> 0);
      output[output_byte++] = static_cast<std::byte>(sample_24 >> 8);
      output[output_byte++] = static_cast<std::byte>(sample_24 >> 16);
    }
    current_sample_++;
  }

  // We wrote everything! Reset, ready for the next frame.
  current_sample_ = -1;
  return std::make_pair(output_byte, true);
}

}  // namespace codecs