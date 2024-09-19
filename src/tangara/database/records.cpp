/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "database/records.hpp"

#include <stdint.h>
#include <sys/_stdint.h>

#include <functional>
#include <iomanip>
#include <iostream>
#include <memory_resource>
#include <sstream>
#include <string>
#include <vector>

#include "cppbor.h"
#include "cppbor_parse.h"
#include "debug.hpp"
#include "esp_log.h"

#include "database/index.hpp"
#include "database/track.hpp"
#include "komihash.h"
#include "memory_resource.hpp"

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

[[maybe_unused]] static const char* kTag = "RECORDS";

static const char kPathPrefix = 'P';
static const char kDataPrefix = 'D';
static const char kHashPrefix = 'H';
static const char kTagHashPrefix = 'T';
static const char kIndexPrefix = 'I';
static const char kFieldSeparator = '\0';

static constexpr auto makePrefix(char p) -> std::string {
  std::string str;
  str += p;
  str += kFieldSeparator;
  return str;
}

auto EncodePathKey(std::string_view path) -> std::string {
  std::stringstream out{};
  out << makePrefix(kPathPrefix);
  out << path;
  return out.str();
}

/* 'D/' */
auto EncodeDataPrefix() -> std::string {
  return makePrefix(kDataPrefix);
}

/* 'D/ 0xACAB' */
auto EncodeDataKey(const TrackId& id) -> std::string {
  return EncodeDataPrefix() + TrackIdToBytes(id);
}

auto EncodeDataValue(const TrackData& track) -> std::string {
  auto* tag_hashes = new cppbor::Map{};  // Free'd by Array's dtor.
  for (const auto& entry : track.individual_tag_hashes) {
    tag_hashes->add(cppbor::Uint{static_cast<uint32_t>(entry.first)},
                    cppbor::Uint{entry.second});
  }
  cppbor::Array val{
      cppbor::Uint{track.id},
      cppbor::Tstr{track.filepath},
      cppbor::Uint{track.tags_hash},
      cppbor::Bool{track.is_tombstoned},
      cppbor::Uint{track.modified_at.first},
      cppbor::Uint{track.modified_at.second},
      tag_hashes,
      cppbor::Uint{track.last_position},
  };
  return val.toString();
}

auto ParseDataValue(const leveldb::Slice& slice) -> std::shared_ptr<TrackData> {
  auto [item, unused, err] = cppbor::parseWithViews(
      reinterpret_cast<const uint8_t*>(slice.data()), slice.size());
  if (!item || item->type() != cppbor::ARRAY) {
    return nullptr;
  }
  auto vals = item->asArray();
  if (vals->size() != 8 || vals->get(0)->type() != cppbor::UINT ||
      vals->get(1)->type() != cppbor::TSTR ||
      vals->get(2)->type() != cppbor::UINT ||
      vals->get(3)->type() != cppbor::SIMPLE ||
      vals->get(4)->type() != cppbor::UINT ||
      vals->get(5)->type() != cppbor::UINT ||
      vals->get(6)->type() != cppbor::MAP  || 
      vals->get(7)->type() != cppbor::UINT) {
    return {};
  }
  auto res = std::make_shared<TrackData>();
  res->id = vals->get(0)->asUint()->unsignedValue();
  res->filepath = vals->get(1)->asViewTstr()->view();
  res->tags_hash = vals->get(2)->asUint()->unsignedValue();
  res->is_tombstoned = vals->get(3)->asBool()->value();
  res->modified_at = std::make_pair<uint16_t, uint16_t>(
      vals->get(4)->asUint()->unsignedValue(),
      vals->get(5)->asUint()->unsignedValue());

  auto tag_hashes = vals->get(6)->asMap();
  for (const auto& entry : *tag_hashes) {
    auto tag = static_cast<Tag>(entry.first->asUint()->unsignedValue());
    res->individual_tag_hashes[tag] = entry.second->asUint()->unsignedValue();
  }

  res->last_position = vals->get(7)->asUint()->unsignedValue();

  return res;
}

/* 'H/ 0xBEEF' */
auto EncodeHashKey(const uint64_t& hash) -> std::string {
  return makePrefix(kHashPrefix) + cppbor::Uint{hash}.toString();
}

auto ParseHashValue(const leveldb::Slice& slice) -> std::optional<TrackId> {
  return BytesToTrackId({slice.data(), slice.size()});
}

auto EncodeHashValue(TrackId id) -> std::string {
  return TrackIdToBytes(id);
}

/* 'T/ 0xBEEF' */
auto EncodeTagHashKey(const uint64_t& hash) -> std::string {
  return makePrefix(kTagHashPrefix) + cppbor::Uint{hash}.toString();
}

/* 'I/' */
auto EncodeAllIndexesPrefix() -> std::string {
  return makePrefix(kIndexPrefix);
}

auto EncodeIndexPrefix(const IndexKey::Header& header) -> std::string {
  std::ostringstream out;
  out << makePrefix(kIndexPrefix);
  cppbor::Array val{
      cppbor::Uint{header.id},
      cppbor::Uint{header.depth},
      cppbor::Uint{header.components_hash},
  };
  out << val.toString() << kFieldSeparator;
  return out.str();
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
auto EncodeIndexKey(const IndexKey& key) -> std::string {
  std::ostringstream out{};

  out << EncodeIndexPrefix(key.header);

  // The component should already be UTF-8 encoded, so just write it.
  if (key.item) {
    out << *key.item << kFieldSeparator;
  }

  if (key.track) {
    out << TrackIdToBytes(*key.track);
  }

  return out.str();
}

auto ParseIndexKey(const leveldb::Slice& slice) -> std::optional<IndexKey> {
  IndexKey result{};

  auto prefix = EncodeAllIndexesPrefix();
  if (!slice.starts_with(prefix)) {
    return {};
  }

  std::string key_data = slice.ToString().substr(prefix.size());
  auto [key, end_of_key, err] = cppbor::parseWithViews(
      reinterpret_cast<const uint8_t*>(key_data.data()), key_data.size());
  if (!key || key->type() != cppbor::ARRAY) {
    return {};
  }
  auto as_array = key->asArray();
  if (as_array->size() != 3 || as_array->get(0)->type() != cppbor::UINT ||
      as_array->get(1)->type() != cppbor::UINT ||
      as_array->get(2)->type() != cppbor::UINT) {
    return {};
  }
  result.header.id = as_array->get(0)->asUint()->unsignedValue();
  result.header.depth = as_array->get(1)->asUint()->unsignedValue();
  result.header.components_hash = as_array->get(2)->asUint()->unsignedValue();

  size_t header_length =
      reinterpret_cast<const char*>(end_of_key) - key_data.data();

  if (header_length == 0 || header_length >= key_data.size()) {
    return {};
  }

  key_data = key_data.substr(header_length + 1);
  size_t last_sep = key_data.find_last_of('\0');

  if (last_sep > 0) {
    result.item = key_data.substr(0, last_sep);
  }

  if (last_sep + 1 < key_data.size()) {
    result.track = BytesToTrackId(key_data.substr(last_sep + 1));
  }

  return result;
}

auto TrackIdToBytes(TrackId id) -> std::string {
  return cppbor::Uint{id}.toString();
}

auto BytesToTrackId(std::span<const char> bytes) -> std::optional<TrackId> {
  auto [res, unused, err] = cppbor::parse(
      reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size());
  if (!res || res->type() != cppbor::UINT) {
    ESP_LOGE(kTag, "Track ID parsing failed!!");
    return {};
  }
  return res->asUint()->unsignedValue();
}

}  // namespace database
