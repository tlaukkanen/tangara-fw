#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "mad.h"
#include "span.hpp"

#include "codec.hpp"

namespace codecs {

class MadMp3Decoder : public ICodec {
 public:
  MadMp3Decoder();
  ~MadMp3Decoder();

  auto CanHandleFile(const std::string& path) -> bool override;
  auto GetOutputFormat() -> OutputFormat override;
  auto ResetForNewStream() -> void override;
  auto SetInput(cpp::span<std::byte> input) -> void override;
  auto GetInputPosition() -> std::size_t override;
  auto ProcessNextFrame() -> cpp::result<bool, ProcessingError> override;
  auto WriteOutputSamples(cpp::span<std::byte> output)
      -> std::pair<std::size_t, bool> override;

 private:
  mad_stream stream_;
  mad_frame frame_;
  mad_synth synth_;

  mad_header header_;
  bool has_decoded_header_;

  int current_sample_;
};

}  // namespace codecs
