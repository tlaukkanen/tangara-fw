#include "stream_info.hpp"

#include <cstdint>
#include <string>

#include "cbor.h"

#include "stream_message.hpp"

namespace audio {

static const std::string kKeyPath = "p";
static const std::string kKeyChannels = "c";
static const std::string kKeyBitsPerSample = "b";
static const std::string kKeySampleRate = "r";

auto StreamInfo::Parse(CborValue& container)
    -> cpp::result<StreamInfo, CborError> {
  CborValue map;
  cbor_value_enter_container(&container, &map);

  CborValue entry;
  StreamInfo ret;

  cbor_value_map_find_value(&map, kKeyPath.c_str(), &entry);
  if (cbor_value_get_type(&entry) != CborInvalidType) {
    char* val;
    size_t len;
    cbor_value_dup_text_string(&entry, &val, &len, NULL);
    ret.path_ = std::string(val, len);
    free(val);
  }
  cbor_value_map_find_value(&map, kKeyChannels.c_str(), &entry);
  if (cbor_value_get_type(&entry) != CborInvalidType) {
    uint64_t val;
    cbor_value_get_uint64(&entry, &val);
    ret.channels_ = val;
  }
  cbor_value_map_find_value(&map, kKeyBitsPerSample.c_str(), &entry);
  if (cbor_value_get_type(&entry) != CborInvalidType) {
    uint64_t val;
    cbor_value_get_uint64(&entry, &val);
    ret.bits_per_sample_ = val;
  }
  cbor_value_map_find_value(&map, kKeySampleRate.c_str(), &entry);
  if (cbor_value_get_type(&entry) != CborInvalidType) {
    uint64_t val;
    cbor_value_get_uint64(&entry, &val);
    ret.sample_rate_ = val;
  }

  return ret;
}

auto StreamInfo::Encode(CborEncoder& enc) -> std::optional<CborError> {
  CborEncoder map;
  size_t num_items = 0 + channels_.has_value() + bits_per_sample_.has_value() +
                     sample_rate_.has_value() + path_.has_value();
  cbor_encoder_create_map(&enc, &map, num_items);

  if (channels_) {
    cbor_encode_text_string(&map, kKeyChannels.c_str(), kKeyChannels.size());
    cbor_encode_uint(&map, channels_.value());
  }
  if (bits_per_sample_) {
    cbor_encode_text_string(&map, kKeyBitsPerSample.c_str(),
                            kKeyBitsPerSample.size());
    cbor_encode_uint(&map, bits_per_sample_.value());
  }
  if (sample_rate_) {
    cbor_encode_text_string(&map, kKeySampleRate.c_str(),
                            kKeySampleRate.size());
    cbor_encode_uint(&map, sample_rate_.value());
  }
  if (path_) {
    cbor_encode_text_string(&map, kKeyPath.c_str(), kKeyPath.size());
    cbor_encode_text_string(&map, path_.value().c_str(), path_.value().size());
  }

  cbor_encoder_close_container(&enc, &map);

  return std::nullopt;
}

}  // namespace audio
