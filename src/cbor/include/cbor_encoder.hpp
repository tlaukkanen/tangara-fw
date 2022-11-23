#pragma once

#include <cstdint>
#include "esp-idf/components/cbor/tinycbor/src/cbor.h"
namespace cbor {

class Encoder {
 public:
  enum ContainerType { CONTAINER_ARRAY, CONTAINER_MAP };
  Encoder(ContainerType type,
          uint32_t container_len,
          uint8_t* buffer,
          size_t buffer_len);

  template <typename T>
  auto WriteKeyValue(const std::string& key, const T& val) -> void {
    WriteValue(key);
    WriteValue(val);
  }

  auto WriteValue(const std::string& val) -> void;
  auto WriteValue(uint32_t val) -> void;
  auto WriteValue(int32_t val) -> void;

  auto Finish() -> cpp::result<size_t, CborError>;

  Encoder(const Encoder&) = delete;
  Encoder& operator=(const Encoder&) = delete;

 private:
  CborEncoder root_encoder_;
  CborEncoder container_encoder_;

  CborError error_ = CborNoError;
};

}  // namespace cbor
