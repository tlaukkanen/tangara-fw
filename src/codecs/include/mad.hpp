#pragma once

#include "codec.hpp"

namespace codecs {

class MadMp3Decoder : public ICodec {
 public:
  MadMp3Decoder();
  ~MadMp3Decoder();

  auto ProcessInput(Result* res, uint8_t* input, std::size_t input_len) -> void;
  auto WriteOutputSamples(Result* res,
                          uint8_t* output,
                          std::size_t output_length) -> void;

 private:
  mad_stream stream_;
  mad_frame frame_;
  mad_synth synth_;

  mad_header header_;
  bool has_decoded_header_;

  int current_sample_ = -1;
};

}  // namespace codecs
