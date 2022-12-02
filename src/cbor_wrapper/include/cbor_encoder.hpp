#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "cbor.h"
#include "result.hpp"

namespace cbor {

enum ContainerType { CONTAINER_ARRAY, CONTAINER_MAP };

class Encoder {
 public:
  Encoder(ContainerType type,
          uint32_t container_len,
          uint8_t* buffer,
          size_t buffer_len);

  auto WriteValue(const std::string& val) -> void;
  auto WriteValue(uint32_t val) -> void;
  auto WriteValue(int32_t val) -> void;

  template <typename T>
  auto WriteKeyValue(const std::string& key, const T&& val) -> void {
    WriteValue(key);
    WriteValue(val);
  }

  template <typename T>
  auto WriteKeyValue(const std::string& key, const std::optional<T>& val)
      -> void {
    if (val) {
      WriteKeyValue<T>(key, val.value());
    }
  }

  auto Finish() -> cpp::result<size_t, CborError>;

  Encoder(const Encoder&) = delete;
  Encoder& operator=(const Encoder&) = delete;

 private:
  uint8_t* buffer_;
  CborEncoder root_encoder_;
  CborEncoder container_encoder_;

  CborError error_ = CborNoError;
};

}  // namespace cbor
