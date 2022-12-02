#include "cbor_decoder.hpp"

#include <cstdint>
#include <string>

#include "cbor.h"
#include "result.hpp"

static const int kDecoderFlags = 0;

namespace cbor {

auto parse_stdstring(const CborValue* val, std::string* out) -> CborError {
  char* buf;
  size_t len;
  CborError err = cbor_value_dup_text_string(val, &buf, &len, NULL);
  if (err != CborNoError) {
    return err;
  }
  *out = std::string(buf, len);
  free(buf);
  return err;
}

auto ArrayDecoder::Create(uint8_t* buffer, size_t buffer_len)
    -> cpp::result<std::unique_ptr<ArrayDecoder>, CborError> {
  auto decoder = std::make_unique<ArrayDecoder>();
  cbor_parser_init(buffer, buffer_len, kDecoderFlags, &decoder->parser_,
                   &decoder->root_);
  if (!cbor_value_is_array(&decoder->root_)) {
    return cpp::fail(CborErrorIllegalType);
  }
  CborError err = cbor_value_enter_container(&decoder->root_, &decoder->it_);
  if (err != CborNoError) {
    return cpp::fail(err);
  }
  return std::move(decoder);
}

auto ArrayDecoder::Create(CborValue& root)
    -> cpp::result<std::unique_ptr<ArrayDecoder>, CborError> {
  auto decoder = std::make_unique<ArrayDecoder>();
  decoder->root_ = root;
  if (!cbor_value_is_array(&decoder->root_)) {
    return cpp::fail(CborErrorIllegalType);
  }

  CborError err = cbor_value_enter_container(&decoder->root_, &decoder->it_);
  if (err != CborNoError) {
    return cpp::fail(err);
  }
  return std::move(decoder);
}

template <>
auto ArrayDecoder::NextValue() -> cpp::result<int64_t, CborError> {
  return NextValue(&cbor_value_is_integer, &cbor_value_get_int);
}
template <>
auto ArrayDecoder::NextValue() -> cpp::result<uint64_t, CborError> {
  return NextValue(&cbor_value_is_unsigned_integer, &cbor_value_get_uint64);
}
template <>
auto ArrayDecoder::NextValue() -> cpp::result<std::string, CborError> {
  return NextValue(&cbor_value_is_byte_string, &parse_stdstring);
}

auto MapDecoder::Create(uint8_t* buffer, size_t buffer_len)
    -> cpp::result<std::unique_ptr<MapDecoder>, CborError> {
  auto decoder = std::make_unique<MapDecoder>();
  cbor_parser_init(buffer, buffer_len, kDecoderFlags, &decoder->parser_,
                   &decoder->root_);
  if (!cbor_value_is_map(&decoder->root_)) {
    return cpp::fail(CborErrorIllegalType);
  }
  CborError err = cbor_value_enter_container(&decoder->root_, &decoder->it_);
  if (err != CborNoError) {
    return cpp::fail(err);
  }
  return std::move(decoder);
}

auto MapDecoder::Create(CborValue& root)
    -> cpp::result<std::unique_ptr<MapDecoder>, CborError> {
  auto decoder = std::make_unique<MapDecoder>();
  decoder->root_ = root;
  if (!cbor_value_is_map(&decoder->root_)) {
    return cpp::fail(CborErrorIllegalType);
  }
  CborError err = cbor_value_enter_container(&decoder->root_, &decoder->it_);
  if (err != CborNoError) {
    return cpp::fail(err);
  }
  return std::move(decoder);
}

template <>
auto MapDecoder::FindValue(const std::string& key) -> std::optional<int64_t> {
  return FindValue(key, &cbor_value_is_integer, &cbor_value_get_int);
}
template <>
auto MapDecoder::FindValue(const std::string& key) -> std::optional<uint64_t> {
  return FindValue(key, &cbor_value_is_unsigned_integer,
                   &cbor_value_get_uint64);
}
template <>
auto MapDecoder::FindValue(const std::string& key)
    -> std::optional<std::string> {
  return FindValue(key, &cbor_value_is_byte_string, &parse_stdstring);
}

}  // namespace cbor
