#pragma once

#include <stdint.h>

#include <functional>
#include <optional>

#include "cbor.h"
#include "result.hpp"

namespace audio {

extern const int kEncoderFlags;
extern const int kDecoderFlags;

enum MessageType {
  TYPE_UNKNOWN,
  TYPE_CHUNK_HEADER,
  TYPE_STREAM_INFO,
};

template <typename Writer>
auto WriteMessage(MessageType type,
                  Writer&& writer,
                  uint8_t* buffer,
                  size_t length) -> cpp::result<size_t, CborError> {
  CborEncoder root;
  CborEncoder container;

  cbor_encoder_init(&root, buffer, length, kEncoderFlags);
  cbor_encoder_create_array(&root, &container, 2);
  cbor_encode_uint(&container, type);

  std::optional<CborError> inner_err = std::invoke(writer, container);
  if (inner_err) {
    return cpp::fail(inner_err.value());
  }

  cbor_encoder_close_container(&root, &container);
  return cbor_encoder_get_buffer_size(&root, buffer);
}

template <typename Result, typename Reader>
auto ReadMessage(Reader&& reader, uint8_t* buffer, size_t length)
    -> cpp::result<Result, CborError> {
  CborParser parser;
  CborValue root;
  CborValue container;

  cbor_parser_init(buffer, length, kDecoderFlags, &parser, &root);
  cbor_value_enter_container(&root, &container);
  // Skip the type header
  cbor_value_advance_fixed(&container);

  return std::invoke(reader, container);
}

auto ReadMessageType(uint8_t* buffer, size_t length) -> MessageType;

}  // namespace audio
