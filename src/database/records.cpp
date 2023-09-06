/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "records.hpp"

#include <stdint.h>

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "cbor.h"
#include "esp_log.h"

#include "index.hpp"
#include "komihash.h"
#include "shared_string.h"
#include "track.hpp"

// As LevelDB is a key-value store, each record in the database consists of a
// key and an optional value.
//
// Values, when present, are always cbor-encoded. This is fast, compact, and
// very easy to evolve over time due to its inclusion of type information.
//
// Keys have a more complicated scheme, as for performance we rely heavily on
// LevelDB's sorted storage format. We must therefore worry about clustering of
// similar records, and the sortability of our encoding format.
//    Each kind of key consists of a a single-byte prefix, then one or more
// fields separated by null (0) bytes. Each field may be cbor-encoded, or may
// use some bespoke encoding; it depends on whether we want to be able to sort
// by that field.
//    For debugging and discussion purposes, we represent field separators
// textually as '/', and write each field as its hex encoding. e.g. a data key
// for the track with id 17 would be written as 'D / 0x11'.

namespace database {

static const char* kTag = "RECORDS";

static const char kDataPrefix = 'D';
static const char kHashPrefix = 'H';
static const char kIndexPrefix = 'I';
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
  // FIXME: With benchmarking to help, we could consider preallocting a small
  // buffer here to do the whole encoding in one pass.
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

/* 'D/' */
auto EncodeDataPrefix() -> OwningSlice {
  char data[2] = {kDataPrefix, kFieldSeparator};
  return OwningSlice({data, 2});
}

/* 'D/ 0xACAB' */
auto EncodeDataKey(const TrackId& id) -> OwningSlice {
  std::ostringstream output;
  output.put(kDataPrefix).put(kFieldSeparator);
  output << TrackIdToBytes(id).data;
  return OwningSlice(output.str());
}

auto EncodeDataValue(const TrackData& track) -> OwningSlice {
  uint8_t* buf;
  std::size_t buf_len = cbor_encode(&buf, [&](CborEncoder* enc) {
    CborEncoder array_encoder;
    CborError err;
    err = cbor_encoder_create_array(enc, &array_encoder, 5);
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      ESP_LOGE(kTag, "encoding err %u", err);
      return;
    }
    err = cbor_encode_int(&array_encoder, track.id());
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      ESP_LOGE(kTag, "encoding err %u", err);
      return;
    }
    err = cbor_encode_text_string(&array_encoder, track.filepath().c_str(),
                                  track.filepath().size());
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      ESP_LOGE(kTag, "encoding err %u", err);
      return;
    }
    err = cbor_encode_uint(&array_encoder, track.tags_hash());
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      ESP_LOGE(kTag, "encoding err %u", err);
      return;
    }
    err = cbor_encode_int(&array_encoder, track.play_count());
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      ESP_LOGE(kTag, "encoding err %u", err);
      return;
    }
    err = cbor_encode_boolean(&array_encoder, track.is_tombstoned());
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

auto ParseDataValue(const leveldb::Slice& slice) -> std::optional<TrackData> {
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
  TrackId id = raw_int;
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

  return TrackData(id, path, hash, play_count, is_tombstoned);
}

/* 'H/ 0xBEEF' */
auto EncodeHashKey(const uint64_t& hash) -> OwningSlice {
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

auto ParseHashValue(const leveldb::Slice& slice) -> std::optional<TrackId> {
  return BytesToTrackId(slice.ToString());
}

auto EncodeHashValue(TrackId id) -> OwningSlice {
  return TrackIdToBytes(id);
}

/* 'I/' */
auto EncodeAllIndexesPrefix() -> OwningSlice {
  char data[2] = {kIndexPrefix, kFieldSeparator};
  return OwningSlice({data, 2});
}

auto AppendIndexHeader(const IndexKey::Header& header, std::ostringstream* out)
    -> void {
  *out << kIndexPrefix << kFieldSeparator;

  // Construct the header.
  uint8_t* buf;
  std::size_t buf_len = cbor_encode(&buf, [&](CborEncoder* enc) {
    CborEncoder array_encoder;
    CborError err;
    err = cbor_encoder_create_array(enc, &array_encoder, 3);
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      ESP_LOGE(kTag, "encoding err %u", err);
      return;
    }
    err = cbor_encode_uint(&array_encoder, header.id);
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      ESP_LOGE(kTag, "encoding err %u", err);
      return;
    }
    err = cbor_encode_uint(&array_encoder, header.depth);
    if (err != CborNoError && err != CborErrorOutOfMemory) {
      ESP_LOGE(kTag, "encoding err %u", err);
      return;
    }
    err = cbor_encode_uint(&array_encoder, header.components_hash);
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
  std::string encoded{reinterpret_cast<char*>(buf), buf_len};
  delete buf;
  *out << encoded << kFieldSeparator;
}

auto EncodeIndexPrefix(const IndexKey::Header& header) -> OwningSlice {
  std::ostringstream out;
  AppendIndexHeader(header, &out);
  return OwningSlice(out.str());
}

/*
 * 'I/0xa2/0x686921/0xb9'
 *                   ^ --- trailer
 *          ^ --- component ("hi!")
 *     ^ -------- header
 *
 *  The components *must* be encoded in a way that is easy to sort
 *  lexicographically. The header and footer do not have this restriction, so
 *  cbor is fine.
 *
 *  We store grouping information within the header; which index, filtered
 *  components. We store disambiguation information in the trailer; just a track
 *  id for now, but could reasonably be something like 'release year' as well.
 */
auto EncodeIndexKey(const IndexKey& key) -> OwningSlice {
  std::ostringstream out;

  // Construct the header.
  AppendIndexHeader(key.header, &out);

  // The component should already be UTF-8 encoded, so just write it.
  if (key.item) {
    out << *key.item;
  }

  // Construct the footer.
  out << kFieldSeparator;
  if (key.track) {
    auto encoded = TrackIdToBytes(*key.track);
    out << encoded.data;
  }
  return OwningSlice(out.str());
}

auto ParseIndexKey(const leveldb::Slice& slice) -> std::optional<IndexKey> {
  IndexKey result{};

  auto prefix = EncodeAllIndexesPrefix();
  if (!slice.starts_with(prefix.data)) {
    return {};
  }

  std::string key_data = slice.ToString().substr(prefix.data.size());
  std::size_t header_length = 0;
  {
    CborParser parser;
    CborValue container;
    CborError err;
    err = cbor_parser_init(reinterpret_cast<const uint8_t*>(key_data.data()),
                           key_data.size(), 0, &parser, &container);
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
    result.header.id = raw_int;
    err = cbor_value_advance(&val);
    if (err != CborNoError || !cbor_value_is_unsigned_integer(&val)) {
      return {};
    }

    err = cbor_value_get_uint64(&val, &raw_int);
    if (err != CborNoError) {
      return {};
    }
    result.header.depth = raw_int;
    err = cbor_value_advance(&val);
    if (err != CborNoError || !cbor_value_is_unsigned_integer(&val)) {
      return {};
    }

    err = cbor_value_get_uint64(&val, &raw_int);
    if (err != CborNoError) {
      return {};
    }
    result.header.components_hash = raw_int;
    err = cbor_value_advance(&val);
    if (err != CborNoError || !cbor_value_at_end(&val)) {
      return {};
    }

    const uint8_t* next_byte = cbor_value_get_next_byte(&val);
    header_length =
        next_byte - reinterpret_cast<const uint8_t*>(key_data.data());
  }

  if (header_length == 0) {
    return {};
  }

  if (header_length >= key_data.size()) {
    return {};
  }

  std::istringstream in(key_data.substr(header_length + 1));
  std::stringbuf buffer{};

  in.get(buffer, kFieldSeparator);
  if (buffer.str().size() > 0) {
    result.item = buffer.str();
  }

  buffer = {};
  in.get(buffer);
  std::string id_str = buffer.str();
  if (id_str.size() > 1) {
    result.track = BytesToTrackId(id_str.substr(1));
  }

  return result;
}

auto TrackIdToBytes(TrackId id) -> OwningSlice {
  uint8_t buf[8];
  CborEncoder enc;
  cbor_encoder_init(&enc, buf, sizeof(buf), 0);
  cbor_encode_uint(&enc, id);
  std::size_t len = cbor_encoder_get_buffer_size(&enc, buf);
  std::string as_str(reinterpret_cast<char*>(buf), len);
  return OwningSlice(as_str);
}

auto BytesToTrackId(const std::string& bytes) -> std::optional<TrackId> {
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
