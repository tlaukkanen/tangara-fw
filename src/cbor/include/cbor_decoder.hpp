#pragma once

#include <stdint.h>

#include <cstdint>

#include "cbor.h"

namespace cbor {

static auto parse_stdstring(CborValue* val, std::string* out) -> CborError {
  uint8_t* buf;
  size_t len;
  CborError err = cbor_value_dup_byte_string(val, &buf, &len, NULL);
  if (err != CborNoError) {
    return err;
  }
  *out = std::move(std::string(buf, len));
  free(buf);
  return err
}

class ArrayDecoder {
 public:
  static auto Create(uint8_t* buffer, size_t buffer_len)
      -> cpp::result<std::unique_ptr<ArrayDecoder>, CborError>;

  static auto Create(CborValue& root)
      -> cpp::result<std::unique_ptr<ArrayDecoder>, CborError>;

  template <typename T>
  auto NextValue() -> cpp::result<T, CborError>;

  template <>
  auto NextValue() -> cpp::result<int64_t, CborError> {
    return NextValue(&cbor_value_is_integer, &cbor_value_get_int);
  }
  template <>
  auto NextValue() -> cpp::result<uint64_t, CborError> {
    return NextValue(&cbor_value_is_unsigned_integer, &cbor_value_get_uint64);
  }
  template <>
  auto NextValue() -> cpp::result<std::string, CborError> {
    return NextValue(&cbor_value_is_byte_string, &parse_stdstring);
  }

  template <typename T>
  auto NextValue(bool (*is_valid)(CborValue*),
                 CborError (*parse)(CborValue*, T*))
      -> cpp::result<T, CborError> {
    if (error_ != CborNoError) {
      return cpp::fail(error_);
    }
    if (!is_valid(&it_)) {
      error_ = CborErrorIllegalType;
      return cpp::fail(error_);
    }
    T ret;
    error_ = parse(&it_, &ret);
    if (error_ != CborNoError) {
      return cpp::fail(error_);
    }
    error_ = cbor_value_advance(&it_);
    if (error_ != CborNoError) {
      return cpp::fail(error_);
    }
    return ret;
  }

  auto Failed() -> CborError { return error_; }

  auto Iterator() -> CborValue& { return it_; }

  ArrayDecoder(const ArrayDecoder&) = delete;
  ArrayDecoder& operator=(const ArrayDecoder&) = delete;

 private:
  CborParser parser_;
  CborValue root_;

  CborValue it_;
  CborError error_ = CborNoError;
};

class MapDecoder {
 public:
  static auto Create(uint8_t* buffer, size_t buffer_len)
      -> cpp::result<std::unique_ptr<MapDecoder>, CborError>;

  static auto Create(CborValue& root)
      -> cpp::result<std::unique_ptr<MapDecoder>, CborError>;

  template <typename T>
  auto FindValue(const std::string& key) -> std::optional<T>;

  template <>
  auto FindValue(const std::string& key) -> std::optional<int64_t> {
    return FindValue(key, &cbor_value_is_integer, &cbor_value_get_int);
  }
  template <>
  auto FindValue(const std::string& key) -> std::optional<uint64_t> {
    return FindValue(key, &cbor_value_is_unsigned_integer,
                     &cbor_value_get_uint64);
  }
  template <>
  auto FindValue(const std::string& key) -> std::optional<std::string> {
    return FindValue(key, &cbor_value_is_byte_string, &parse_stdstring);
  }

  template <typename T>
  auto FindValue(const std::string& key,
                 bool (*is_valid)(CborValue*),
                 CborError (*parse)(CborValue*, T*)) -> std::optional<T> {
    if (error_ != CborNoError) {
      return {};
    }
    if (cbor_value_map_find_value(&it_, key.c_str(), &val) != CborNoError) {
      return {};
    }
    if (!is_valid(&val)) {
      error_ = CborErrorIllegalType;
      return {};
    }
    T ret;
    error_ = parse(&val, &ret);
    if (error_ != CborNoError) {
      return cpp::fail(error_);
    }
    return ret;
  }

  auto Failed() -> CborError { return error_; }

  MapDecoder(const MapDecoder&) = delete;
  MapDecoder& operator=(const MapDecoder&) = delete;

 private:
  CborParser parser_;
  CborValue root_;

  CborValue it_;
  CborError error_ = CborNoError;
};

}  // namespace cbor
