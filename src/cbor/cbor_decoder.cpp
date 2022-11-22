#include "cbor_decoder.hpp"
#include <cstdint>
#include "esp-idf/components/cbor/tinycbor/src/cbor.h"
#include "include/cbor_decoder.hpp"

namespace cbor {

static auto ArrayDecoder::Create(uint8_t *buffer, size_t buffer_len) -> cpp::result<std::unique_ptr<ArrayDecoder>, CborError> {
  auto decoder = std::make_unique<ArrayDecoder>();
  cbor_parser_init(buffer, buffer_len, &decoder->parser_, &decoder->root_);
  if (!cbor_value_is_array(&decoder->root_)) {
    return cpp::fail(CborErrorIllegalType);
  }
  CborError err = cbor_value_enter_container(&decoder->root_, &decoder->it_);
  if (err != CborNoError) {
    return cpp::fail(err);
  }
  return std::move(decoder);
}

static auto MapDecoder::Create(uint8_t *buffer, size_t buffer_len) -> cpp::result<std::unique_ptr<MapDecoder>, CborError> {
  auto decoder = std::make_unique<MapDecoder>();
  cbor_parser_init(buffer, buffer_len, &decoder->parser_, &decoder->root_);
  if (!cbor_value_is_map(&decoder->root_)) {
    return cpp::fail(CborErrorIllegalType);
  }
  CborError err = cbor_value_enter_container(&decoder->root_, &decoder->it_);
  if (err != CborNoError) {
    return cpp::fail(err);
  }
  return std::move(decoder);
}

auto MapDecoder::FindString(const std::string &key) -> std::optional<std::string> {
  CborValue val;
  if (error_ != CborNoError) {
    return {};
  }
  if (cbor_value_map_find_value(&it_, key.c_str(), &val) != CborNoError) {
    return {};
  }
  if (!cbor_value_is_byte_string(&val)) {
    error_ = CborErrorIllegalType;
    return {};
  }
  uint8_t *buf; size_t len;
  error_ = cbor_value_dup_byte_string(&val, &buf, &len, NULL);
  if (error_ != CborNoError) {
    return cpp::fail(error_);
  }
  std::string ret(buf, len);
  free(buf);
  return ret;
}

auto MapDecoder::FindUnsigned(const std::string &key) -> std::optional<uint32_t> {
  CborValue val;
  if (error_ != CborNoError) {
    return {};
  }
  if (cbor_value_map_find_value(&it_, key.c_str(), &val) != CborNoError) {
    return {};
  }
  if (!cbor_value_is_unsigned_integer(&val)) {
    error_ = CborErrorIllegalType;
    return {};
  }
  uint64_t ret;
  error_ = cbor_value_get_uint64(&val, &ret);
  if (error_ != CborNoError) {
    return cpp::fail(error_);
  }
  return ret;
}

auto MapDecoder::FindSigned(const std::string &key) -> std::optional<int32_t> {
  CborValue val;
  if (error_ != CborNoError) {
    return {};
  }
  if (cbor_value_map_find_value(&it_, key.c_str(), &val) != CborNoError) {
    // Don't store this as an error; missing keys are fine.
    return {};
  }
  if (!cbor_value_is_integer(&val)) {
    error_ = CborErrorIllegalType;
    return {};
  }
  int32_t ret;
  error_ = cbor_value_get_int(&val, &ret);
  if (error_ != CborNoError) {
    return cpp::fail(error_);
  }
  return ret;
}

} // namespace cbor
