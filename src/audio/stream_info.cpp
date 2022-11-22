#include "stream_info.hpp"
#include "stream_message.hpp"
#include <cstdint>
#include "esp-idf/components/cbor/tinycbor/src/cbor.h"

namespace audio {

  static const char* kKeyPath = "p";
  static const char* kKeyChannels = "c";
  static const char* kKeyBitsPerSample = "b";
  static const char* kKeySampleRate = "r";

  static auto find_uint64(CborValue &map, char *key) -> cpp::optional<uint64_t> {
    CborValue val;
    cbor_value_map_find_value(&map, key, &val);
    if (cbor_value_is_unsigned_integer(&val)) {
      uint64_t raw_val;
      cbor_value_get_uint64(&val, &raw_val);
      return raw_val;
    }
    return {};
  }


  static auto write_uint64(CborEncoder &map, const char *key, const optional<uint64_t> &val) -> cpp::result<void, StreamInfo::EncodeError> {
    if (val) {
      cbor_encode_byte_string(&map, key, 1);
      cbor_encode_uint(&map, *val);
    }
    return {};
  }

static auto StreamInfo::Create(const uint8_t *buffer, size_t length) -> cpp::result<StreamInfo, ParseError> {
  CborParser parser;
  CborValue value;

  cbor_parser_init(buffer, len, 0, &parser, &value);

  uint8_t type = 0;
  if (!cbor_value_is_integer(&value)
      || !cbor_value_get_integer(&value, &type)
      || type != STREAM_INFO) {
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
  channels_ = find_uint64(map, kKeyChannels);
  bits_per_sample_ = find_uint64(map, kKeyBitsPerSample);
  sample_rate_ = find_uint64(map, kKeySampleRate);

  CborValue val;
  cbor_value_map_find_value(&map, kKeyPath, &val);
  if (cbor_value_is_text_string(&val)) {
    size_t len;
    char *str;
    cbor_value_dup_text_string(&val, &str, &len, &val);
    path_ = std::string(str, len);
    free(str);
  }
}

auto StreamInfo::WriteToStream(CborEncoder encoder) -> cpp::result<void, EncodeError> {
  cbor_encode_int(&encoder, STREAM_INFO);

  CborEncoder map;
  cbor_encoder_create_map(&encoder, &map, length);

  write_uint64(&map, kKeyChannels, channels_);
  write_uint64(&map, kKeyBitsPerSample, bits_per_sample_);
  write_uint64(&map, kKeySampleRate, sample_rate_);

  if (path_) {
    cbor_encode_text_string(&map, path_->c_str(), path_->size());
  }

  cbor_encoder_close_container(&encoder, &map);
}

} // namespace audio
