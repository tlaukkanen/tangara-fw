#include "stream_message.hpp"

#include <cstdint>

#include "cbor.h"

namespace audio {

const int kEncoderFlags = 0;
const int kDecoderFlags = 0;

auto ReadMessageType(uint8_t* buffer, size_t length) -> MessageType {
  CborParser parser;
  CborValue root;
  CborValue container;

  cbor_parser_init(buffer, length, kDecoderFlags, &parser, &root);
  cbor_value_enter_container(&root, &container);

  uint64_t header = 0;
  cbor_value_get_uint64(&container, &header);

  return static_cast<MessageType>(header);
}

}  // namespace audio
