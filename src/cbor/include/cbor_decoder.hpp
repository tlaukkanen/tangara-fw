#pragma once

#include <cstdint>
namespace cbor {

  class ArrayDecoder {
    public:
      static auto Create(uint8_t *buffer, size_t buffer_len) -> cpp::result<std::unique_ptr<ArrayDecoder>, CborError>;

      auto ParseString() -> cpp::result<std::string, CborError>;
      auto ParseUnsigned() -> cpp::result<uint32_t, CborError>;
      auto ParseSigned() -> cpp::result<int32_t, CborError>;

      auto Failed() -> CborError { return error_; }

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
      static auto Create(uint8_t *buffer, size_t buffer_len) -> cpp::result<std::unique_ptr<MapDecoder>, CborError>;

      auto FindString(const std::string &key) -> std::optional<std::string>;
      auto FindUnsigned(const std::string &key) -> std::optional<uint32_t>;
      auto FindSigned(const std::string &key) -> std::optional<int32_t>;

      auto Failed() -> CborError { return error_; }

      MapDecoder(const MapDecoder&) = delete;
      MapDecoder& operator=(const MapDecoder&) = delete;
    private:
      CborParser parser_;
      CborValue root_;

      CborValue it_;
      CborError error_ = CborNoError;
  };


} // namespace cbor
