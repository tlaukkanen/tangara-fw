#include "stream_info.hpp"
#include <cstdint>
#include "cbor_decoder.hpp"
#include "esp-idf/components/cbor/tinycbor/src/cbor.h"
#include "stream_message.hpp"

namespace audio {

static const char* kKeyPath = "p";
static const char* kKeyChannels = "c";
static const char* kKeyBitsPerSample = "b";
static const char* kKeySampleRate = "r";

static auto StreamInfo::Create(const uint8_t* buffer, size_t length)
    -> cpp::result<StreamInfo, ParseError> {
  CborParser parser;
  CborValue value;

  cbor_parser_init(buffer, len, 0, &parser, &value);

  uint8_t type = 0;
  if (!cbor_value_is_integer(&value) ||
      !cbor_value_get_integer(&value, &type) || type != STREAM_INFO) {
    return cpp::fail(WRONG_TYPE);
  }

  cbor_value_advance_fixed(&value);

  if (!cbor_value_is_map(&value)) {
    return cpp::fail(MISSING_MAP);
  }

  return StreamInfo(value);
}

StreamInfo::StreamInfo(CborValue& map) {
  // TODO: this method is n^2, which seems less than ideal. But you don't do it
  // that frequently, so maybe it's okay? Needs investigation.
  cbor::MapDecoder decoder(map);
  channels_ = decoder.FindValue(kKeyChannels);
  bits_per_sample_ = decoder.FindValue(kKeyBitsPerSample);
  sample_rate_ = decoder.FindValue(kKeySampleRate);
  path_ = decoder.FindValue(kKeyPath);
}

auto StreamInfo::WriteToMap(cbor::Encoder& map_encoder)
    -> cpp::result<size_t, EncodeError> {
  CborEncoder map;
  map_encoder.WriteKeyValue(kKeyChannels, channels_);
  map_encoder.WriteKeyValue(kKeyBitsPerSample, bits_per_sample_);
  map_encoder.WriteKeyValue(kKeySampleRate, sample_rate_);
  map_encoder.WriteKeyValue(kKeyPath, path_);
  return map_encoder.Finish();
}

}  // namespace audio
