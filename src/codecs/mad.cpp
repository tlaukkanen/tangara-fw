#include "mad.hpp"
#include <cstdint>

#include "mad.h"

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

      auto MadMp3Decoder::CanHandleExtension(std::string extension) -> bool {
      return extension == "mp3";
      }

      auto GetOutputFormat() -> OutputFormat {
        return OutputFormat {
          .num_channels = synth_.pcm.channels,
          .bits_per_sample = 24,
          .sample_rate_hz = synth_.pcm.samplerate,
        };
      }

      auto MadMp3Decoder::Process(
          uint8_t *input,
          std::size_t input_len,
          uint8_t *output,
	  std::size_t output_length) -> cpp::result<Result, Error> {

        Result res {
          .need_more_input = false,
          .input_processed = 0,
          .flush_output = false,
          .output_written = 0,
        }
        while (true) {
          // Only process more of the input if we're done sending off the
          // samples for the previous frame.
          if (current_sample_ == -1) {
            ProcessInput(&res, input, input_len);
          }

          // Write PCM samples to the output buffer. This always needs to be
          // done, even if we ran out of input, so that we don't keep the last
          // few samples buffered if the input stream has actually finished.
          WriteOutputSamples(&res, output, output_length);

          if (res.need_more_input || res.flush_output) {
            return res;
          }
      }

      auto MadMp3Decoder::ProcessInput(
          Result *res, uint8_t *input, std::size_t input_len) -> void {

        if (input != stream_.buffer) {
          mad_stream_buffer(&stream_, input, input_len);
        }

          if (!has_decoded_header_) {
            // The header of any given frame should be representative of the
            // entire stream, so only need to read it once.
            mad_header_decode(&header_, &stream);
            has_decoded_header_ = true;

            // TODO: Use the info in the header for something. I think the
            // duration will help with seeking?
          }


          // Decode the next frame. To signal errors, this returns -1 and
          // stashes an error code in the stream structure.
          if (mad_frame_decode(&frame_, &stream_) < 0) {
            if (MAD_RECOVERABLE(stream_.error)) {
              // Recoverable errors are usually malformed parts of the stream.
              // We can recover from them by just retrying the decode.
              continue;
            }

            if (stream_.error = MAD_ERROR_BUFLEN) {
              // The decoder ran out of bytes before it completed a frame. We
              // need to return back to the caller to give us more data. Note
              // that there might still be some unused data in the input, so we
              // should calculate that amount and return it.
              size_t remaining_bytes = stream.bufend - stream_.next_frame;
              return remaining_bytes;
            }

            // The error is unrecoverable. Give up.
            return cpp::fail(MALFORMED_DATA);
          }

          // We've successfully decoded a frame!
          // Now we need to synthesize PCM samples based on the frame, and send
          // them downstream.
          mad_synth_frame(&synth_, &frame_);
          up_to_sample = 0;
      }

      auto MadMp3Decoder::WriteOutputSamples(
          Result *res,
          uint8_t *output,
	  std::size_t output_length) -> void {
        size_t output_byte = 0;
        // First ensure that we actually have some samples to send off.
        if (current_sample_ < 0) {
          return;
        }
        res->flush_output = true;

        while (current_sample_ < synth_.pcm.length) {
          if (output_byte + (3 * synth_.pcm.channels) >= output_length) {
            res->output_written = output_byte;
            return;
          }

          for (int channel = 0; channel < synth_.pcm.channels; channel++) {
            uint32_t sample_24 = scaleTo24Bits(synth_.pcm.samples[channel][sample]);
            output[output_byte++] = (sample_24 >> 0) & 0xff;
            output[output_byte++] = (sample_24 >> 8) & 0xff;
            output[output_byte++] = (sample_24 >> 16) & 0xff;
          }
          current_sample_++;
        }

        // We wrote everything! Reset, ready for the next frame.
        current_sample_ = -1;
        res->output_written = output_byte;
      }

} // namespace codecs
