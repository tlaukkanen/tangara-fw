#include "stream_info.hpp"

#include <cstdint>
#include <string>

#include "cbor.h"

#include "cbor_decoder.hpp"
#include "cbor_encoder.hpp"
#include "stream_message.hpp"

namespace audio {

static const std::string kKeyPath = "p";
static const std::string kKeyChannels = "c";
static const std::string kKeyBitsPerSample = "b";
static const std::string kKeySampleRate = "r";

auto StreamInfo::Create(const uint8_t* buffer, size_t length)
    -> cpp::result<StreamInfo, ParseError> {
  CborParser parser;
  CborValue value;

  cbor_parser_init(buffer, length, 0, &parser, &value);

  int type = 0;
  if (!cbor_value_is_integer(&value) || !cbor_value_get_int(&value, &type) ||
      type != TYPE_STREAM_INFO) {
    return cpp::fail(WRONG_TYPE);
  }

  cbor_value_advance_fixed(&value);

  if (!cbor_value_is_map(&value)) {
    return cpp::fail(MISSING_MAP);
  }

  auto map_decoder = cbor::MapDecoder::Create(value);
  if (map_decoder.has_value()) {
    return StreamInfo(map_decoder.value().get());
  }
  return cpp::fail(CBOR_ERROR);
}

StreamInfo::StreamInfo(cbor::MapDecoder* decoder) {
  // TODO: this method is n^2, which seems less than ideal. But you don't do it
  // that frequently, so maybe it's okay? Needs investigation.
  channels_ = decoder->FindValue<uint64_t>(kKeyChannels);
  bits_per_sample_ = decoder->FindValue<uint64_t>(kKeyBitsPerSample);
  sample_rate_ = decoder->FindValue<uint64_t>(kKeySampleRate);
  path_ = decoder->FindValue<std::string>(kKeyPath);
}

auto StreamInfo::WriteToMap(cbor::Encoder& map_encoder)
    -> cpp::result<size_t, CborError> {
  map_encoder.WriteKeyValue<uint64_t>(kKeyChannels, channels_);
  map_encoder.WriteKeyValue<uint64_t>(kKeyBitsPerSample, bits_per_sample_);
  map_encoder.WriteKeyValue<uint64_t>(kKeySampleRate, sample_rate_);
  map_encoder.WriteKeyValue<std::string>(kKeyPath, path_);
  return map_encoder.Finish();
}

}  // namespace audio
