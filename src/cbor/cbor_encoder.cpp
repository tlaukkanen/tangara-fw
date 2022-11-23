#include "cbor_encoder.hpp"
#include <cstdint>
#include "esp-idf/components/cbor/tinycbor/src/cbor.h"

namespace cbor {

static const int kEncoderFlags = 0;

Encoder::Encoder(ContainerType type,
                 uint32_t container_len,
                 uint8_t* buffer,
                 size_t buffer_len) {
  cbor_encoder_init(&root_encoder, buffer, buffer_len, kEncoderFlags);
  switch (type) {
    case CONTAINER_ARRAY:
      error_ = cbor_encoder_create_array(&encoder, &container_encoder_,
                                         container_len);
      break;
    case CONTAINER_MAP:
      error_ =
          cbor_encoder_create_map(&encoder, &container_encoder_, container_len);
      break;
  }
}

auto Encoder::WriteValue(const std::string& val) -> void {
  if (error_ != CborNoError) {
    return;
  }
  error_ =
      cbor_encode_byte_string(&container_encoder_, val.c_str(), val.size());
}

auto Encoder::WriteValue(uint32_t val) -> void {
  if (error_ != CborNoError) {
    return;
  }
  error_ = cbor_encode_uint(&container_encoder_, val);
}

auto Encoder::WriteValue(int32_t val) -> void {
  if (error_ != CborNoError) {
    return;
  }
  error_ = cbor_encode_int(&container_encoder_, val);
}

auto Encoder::Finish() -> cpp::result<size_t, CborError> {
  if (error_ != CborNoError) {
    return cpp::fail(error_);
  }
  if (CborError final_error =
          cbor_encoder_close_container(&root_encoder, &container_encoder_) !=
          CborNoError) {
    return cpp::fail(final_error);
  }
  return cbor_encoder_get_buffer_size(&root_encoder);
}

}  // namespace cbor
