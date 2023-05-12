#include "records.hpp"

#include <stdint.h>

#include <sstream>
#include <vector>

#include "cbor.h"
#include "esp_log.h"

#include "song.hpp"

namespace database {

static const char* kTag = "RECORDS";

static const char kDataPrefix = 'D';
static const char kHashPrefix = 'H';
static const char kFieldSeparator = '\0';

/*
 * Helper function for allocating an appropriately-sized byte buffer, then
 * encoding data into it.
 *
 * 'T' should be a callable that takes a CborEncoder* as
 * an argument, and stores values within that encoder. 'T' will be called
 * exactly twice: first to detemine the buffer size, and then second to do the
 * encoding.
 *
 * 'out_buf' will be set to the location of newly allocated memory; it is up to
 * the caller to free it. Returns the size of 'out_buf'.
 */
template <typename T>
auto cbor_encode(uint8_t** out_buf, T fn) -> std::size_t {
  // First pass: work out how many bytes we will encode into.
  CborEncoder size_encoder;
  cbor_encoder_init(&size_encoder, NULL, 0, 0);
  std::invoke(fn, &size_encoder);
  std::size_t buf_size = cbor_encoder_get_extra_bytes_needed(&size_encoder);

  // Second pass: do the encoding.
  CborEncoder encoder;
  *out_buf = new uint8_t[buf_size];
  cbor_encoder_init(&encoder, *out_buf, buf_size, 0);
  std::invoke(fn, &encoder);

  return buf_size;
}

OwningSlice::OwningSlice(std::string d) : data(d), slice(data) {}

auto CreateDataPrefix() -> OwningSlice {
  char data[2] = {kDataPrefix, kFieldSeparator};
  return OwningSlice({data, 2});
}

auto CreateDataKey(const SongId& id) -> OwningSlice {
  std::ostringstream output;
  output.put(kDataPrefix).put(kFieldSeparator);
  output << SongIdToBytes(id).data;
  return OwningSlice(output.str());
}

auto CreateDataValue(const SongData& song) -> OwningSlice {
  uint8_t* buf;
  std::size_t buf_len = cbor_encode(&buf, [&](CborEncoder* enc) {
    CborEncoder array_encoder;
    CborError err;
    err = cbor_encoder_create_array(enc, &array_encoder, 5);
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      ESP_LOGE(kTag, "encoding err %u", err);
      return;
    }
    err = cbor_encode_int(&array_encoder, song.id());
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      ESP_LOGE(kTag, "encoding err %u", err);
      return;
    }
    err = cbor_encode_text_string(&array_encoder, song.filepath().c_str(),
                                  song.filepath().size());
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      ESP_LOGE(kTag, "encoding err %u", err);
      return;
    }
    err = cbor_encode_uint(&array_encoder, song.tags_hash());
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      ESP_LOGE(kTag, "encoding err %u", err);
      return;
    }
    err = cbor_encode_int(&array_encoder, song.play_count());
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      ESP_LOGE(kTag, "encoding err %u", err);
      return;
    }
    err = cbor_encode_boolean(&array_encoder, song.is_tombstoned());
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      ESP_LOGE(kTag, "encoding err %u", err);
      return;
    }
    err = cbor_encoder_close_container(enc, &array_encoder);
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      ESP_LOGE(kTag, "encoding err %u", err);
      return;
    }
  });
  std::string as_str(reinterpret_cast<char*>(buf), buf_len);
  delete buf;
  return OwningSlice(as_str);
}

auto ParseDataValue(const leveldb::Slice& slice) -> std::optional<SongData> {
  CborParser parser;
  CborValue container;
  CborError err;
  err = cbor_parser_init(reinterpret_cast<const uint8_t*>(slice.data()),
                         slice.size(), 0, &parser, &container);
  if (err != CborNoError || !cbor_value_is_container(&container)) {
    return {};
  }

  CborValue val;
  err = cbor_value_enter_container(&container, &val);
  if (err != CborNoError || !cbor_value_is_unsigned_integer(&val)) {
    return {};
  }

  uint64_t raw_int;
  err = cbor_value_get_uint64(&val, &raw_int);
  if (err != CborNoError) {
    return {};
  }
  SongId id = raw_int;
  err = cbor_value_advance(&val);
  if (err != CborNoError || !cbor_value_is_text_string(&val)) {
    return {};
  }

  char* raw_path;
  std::size_t len;
  err = cbor_value_dup_text_string(&val, &raw_path, &len, &val);
  if (err != CborNoError || !cbor_value_is_unsigned_integer(&val)) {
    return {};
  }
  std::string path(raw_path, len);
  delete raw_path;

  err = cbor_value_get_uint64(&val, &raw_int);
  if (err != CborNoError) {
    return {};
  }
  uint64_t hash = raw_int;
  err = cbor_value_advance(&val);
  if (err != CborNoError || !cbor_value_is_unsigned_integer(&val)) {
    return {};
  }

  err = cbor_value_get_uint64(&val, &raw_int);
  if (err != CborNoError) {
    return {};
  }
  uint32_t play_count = raw_int;
  err = cbor_value_advance(&val);
  if (err != CborNoError || !cbor_value_is_boolean(&val)) {
    return {};
  }

  bool is_tombstoned;
  err = cbor_value_get_boolean(&val, &is_tombstoned);
  if (err != CborNoError) {
    return {};
  }

  return SongData(id, path, hash, play_count, is_tombstoned);
}

auto CreateHashKey(const uint64_t& hash) -> OwningSlice {
  std::ostringstream output;
  output.put(kHashPrefix).put(kFieldSeparator);

  uint8_t buf[16];
  CborEncoder enc;
  cbor_encoder_init(&enc, buf, sizeof(buf), 0);
  cbor_encode_uint(&enc, hash);
  std::size_t len = cbor_encoder_get_buffer_size(&enc, buf);
  output.write(reinterpret_cast<char*>(buf), len);

  return OwningSlice(output.str());
}

auto ParseHashValue(const leveldb::Slice& slice) -> std::optional<SongId> {
  return BytesToSongId(slice.ToString());
}

auto CreateHashValue(SongId id) -> OwningSlice {
  return SongIdToBytes(id);
}

auto SongIdToBytes(SongId id) -> OwningSlice {
  uint8_t buf[8];
  CborEncoder enc;
  cbor_encoder_init(&enc, buf, sizeof(buf), 0);
  cbor_encode_uint(&enc, id);
  std::size_t len = cbor_encoder_get_buffer_size(&enc, buf);
  std::string as_str(reinterpret_cast<char*>(buf), len);
  return OwningSlice(as_str);
}

auto BytesToSongId(const std::string& bytes) -> std::optional<SongId> {
  CborParser parser;
  CborValue val;
  cbor_parser_init(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size(),
                   0, &parser, &val);
  if (!cbor_value_is_unsigned_integer(&val)) {
    return {};
  }
  uint64_t raw_id;
  cbor_value_get_uint64(&val, &raw_id);
  return raw_id;
}

}  // namespace database
