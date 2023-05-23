/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include "cbor.h"
#include "result.hpp"
#include "span.hpp"

namespace audio {

extern const int kEncoderFlags;
extern const int kDecoderFlags;

enum MessageType {
  TYPE_UNKNOWN,
  TYPE_CHUNK_HEADER,
  TYPE_STREAM_INFO,
};

template <typename Writer>
auto WriteMessage(MessageType type, Writer&& writer, cpp::span<std::byte> data)
    -> cpp::result<size_t, CborError> {
  CborEncoder root;
  CborEncoder container;
  uint8_t* cast_data = reinterpret_cast<uint8_t*>(data.data());

  cbor_encoder_init(&root, cast_data, data.size(), kEncoderFlags);
  cbor_encoder_create_array(&root, &container, 2);
  cbor_encode_uint(&container, type);

  std::optional<CborError> inner_err = std::invoke(writer, container);
  if (inner_err) {
    return cpp::fail(inner_err.value());
  }

  cbor_encoder_close_container(&root, &container);
  return cbor_encoder_get_buffer_size(&root, cast_data);
}

template <typename Result, typename Reader>
auto ReadMessage(Reader&& reader, cpp::span<std::byte> data)
    -> cpp::result<Result, CborError> {
  CborParser parser;
  CborValue root;
  CborValue container;

  cbor_parser_init(reinterpret_cast<uint8_t*>(data.data()), data.size(),
                   kDecoderFlags, &parser, &root);
  cbor_value_enter_container(&root, &container);
  // Skip the type header
  cbor_value_advance_fixed(&container);

  return std::invoke(reader, container);
}

auto WriteTypeOnlyMessage(MessageType type, cpp::span<std::byte> data)
    -> cpp::result<size_t, CborError>;
auto ReadMessageType(cpp::span<std::byte> msg) -> MessageType;
auto GetAdditionalData(cpp::span<std::byte> msg) -> cpp::span<std::byte>;

}  // namespace audio
