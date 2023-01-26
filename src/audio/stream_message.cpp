#include "stream_message.hpp"

#include <cstdint>

#include "cbor.h"
#include "span.hpp"

namespace audio {

const int kEncoderFlags = 0;
const int kDecoderFlags = 0;

auto WriteTypeOnlyMessage(MessageType type, cpp::span<std::byte> data)
    -> cpp::result<size_t, CborError> {
  CborEncoder root;
  CborEncoder container;
  uint8_t* cast_data = reinterpret_cast<uint8_t*>(data.data());

  cbor_encoder_init(&root, cast_data, data.size(), kEncoderFlags);
  cbor_encode_uint(&container, type);

  return cbor_encoder_get_buffer_size(&root, cast_data);
}

auto ReadMessageType(cpp::span<std::byte> msg) -> MessageType {
  CborParser parser;
  CborValue root;
  CborValue container;

  cbor_parser_init(reinterpret_cast<uint8_t*>(msg.data()), msg.size(),
                   kDecoderFlags, &parser, &root);

  uint64_t header = 0;
  if (cbor_value_is_container(&root)) {
    cbor_value_enter_container(&root, &container);
    cbor_value_get_uint64(&container, &header);
  } else {
    cbor_value_get_uint64(&root, &header);
  }

  return static_cast<MessageType>(header);
}

auto GetAdditionalData(cpp::span<std::byte> msg) -> cpp::span<std::byte> {
  CborParser parser;
  CborValue root;
  uint8_t* cast_data = reinterpret_cast<uint8_t*>(msg.data());

  cbor_parser_init(cast_data, msg.size(), kDecoderFlags, &parser, &root);

  while (!cbor_value_at_end(&root)) {
    cbor_value_advance(&root);
  }

  const uint8_t* remaining = cbor_value_get_next_byte(&root);
  size_t header_size = remaining - cast_data;

  return cpp::span<std::byte>(msg.data() + header_size,
                              msg.size() - header_size);
}

}  // namespace audio
