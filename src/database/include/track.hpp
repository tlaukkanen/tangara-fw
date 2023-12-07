/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>
#include <sys/_stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "leveldb/db.h"
#include "memory_resource.hpp"
#include "span.hpp"

namespace database {

/*
 * Uniquely describes a single track within the database. This value will be
 * consistent across database updates, and should ideally (but is not guaranteed
 * to) endure even across a track being removed and re-added.
 *
 * Four billion tracks should be enough for anybody.
 */
typedef uint32_t TrackId;

/*
 * Audio file encodings that we are aware of. Used to select an appropriate
 * decoder at play time.
 *
 * Values of this enum are persisted in this database, so it is probably never a
 * good idea to change the int representation of an existing value.
 */
enum class Container {
  kUnsupported = 0,
  kMp3 = 1,
  kWav = 2,
  kOgg = 3,
  kFlac = 4,
  kOpus = 5,
};

enum class Tag {
  kTitle = 0,
  kArtist = 1,
  kAlbum = 2,
  kAlbumTrack = 3,
  kGenre = 4,
  kDuration = 5,
};

auto TagToString(Tag t) -> std::string;

/*
 * Owning container for tag-related track metadata that was extracted from a
 * file.
 */
class TrackTags {
 public:
  TrackTags()
      : encoding_(Container::kUnsupported), tags_(&memory::kSpiRamResource) {}

  TrackTags(const TrackTags& other) = delete;
  TrackTags& operator=(TrackTags& other) = delete;

  bool operator==(const TrackTags&) const = default;

  auto encoding() const -> Container { return encoding_; };
  auto encoding(Container e) -> void { encoding_ = e; };

  std::optional<int> channels;
  std::optional<int> sample_rate;
  std::optional<int> bits_per_sample;

  std::optional<int> duration;

  auto set(const Tag& key, const std::pmr::string& val) -> void;
  auto at(const Tag& key) const -> std::optional<std::pmr::string>;
  auto operator[](const Tag& key) const -> std::optional<std::pmr::string>;

  auto tags() const -> const std::pmr::unordered_map<Tag, std::pmr::string>& {
    return tags_;
  }

  /*
   * Returns a hash of the 'identifying' tags of this track. That is, a hash
   * that can be used to determine if one track is likely the same as another,
   * across things like re-encoding, re-mastering, or moving the underlying
   * file.
   */
  auto Hash() const -> uint64_t;

 private:
  Container encoding_;
  std::pmr::unordered_map<Tag, std::pmr::string> tags_;
};

/*
 * Owning container for all of the metadata we store for a particular track.
 * This includes two main kinds of metadata:
 *  1. static(ish) attributes, such as the id, path on disk, hash of the tags
 *  2. dynamic attributes, such as the number of times this track has been
 *  played.
 *
 * Because a TrackData is immutable, it is thread safe but will not reflect any
 * changes to the dynamic attributes that may happen after it was obtained.
 *
 * Tracks may be 'tombstoned'; this indicates that the track is no longer
 * present at its previous location on disk, and we do not have any existing
 * files with a matching tags_hash. When this is the case, we ignore this
 * TrackData for most purposes. We keep the entry in our database so that we can
 * properly restore dynamic attributes (such as play count) if the track later
 * re-appears on disk.
 */
struct TrackData {
 public:
  TrackData()
      : id(0),
        filepath(),
        tags_hash(0),
        individual_tag_hashes(&memory::kSpiRamResource),
        is_tombstoned(false),
        modified_at() {}

  TrackId id;
  std::pmr::string filepath;
  uint64_t tags_hash;
  std::pmr::unordered_map<Tag, uint64_t> individual_tag_hashes;
  bool is_tombstoned;
  std::pair<uint16_t, uint16_t> modified_at;

  TrackData(TrackData&& other) = delete;
  TrackData& operator=(TrackData& other) = delete;

  bool operator==(const TrackData&) const = default;
};

/*
 * Immutable and owning combination of a track's tags and metadata.
 *
 * Note that instances of this class may have a fairly large memory impact, due
 * to the large number of strings they own. Prefer to query the database again
 * (which has its own caching layer), rather than retaining Track instances for
 * a long time.
 */
class Track {
 public:
  Track(std::shared_ptr<TrackData>& data, std::shared_ptr<TrackTags> tags)
      : data_(data), tags_(tags) {}

  Track(Track& other) = delete;
  Track& operator=(Track& other) = delete;

  bool operator==(const Track&) const = default;

  auto data() const -> const TrackData& { return *data_; }
  auto tags() const -> const TrackTags& { return *tags_; }

  auto TitleOrFilename() const -> std::pmr::string;

 private:
  std::shared_ptr<const TrackData> data_;
  std::shared_ptr<TrackTags> tags_;
};

}  // namespace database
