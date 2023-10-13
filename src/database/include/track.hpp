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
 * Immutable owning container for all of the metadata we store for a particular
 * track. This includes two main kinds of metadata:
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
class TrackData {
 private:
  const TrackId id_;
  const std::pmr::string filepath_;
  const uint64_t tags_hash_;
  const bool is_tombstoned_;
  const std::pair<uint16_t, uint16_t> modified_at_;

 public:
  /* Constructor used when adding new tracks to the database. */
  TrackData(TrackId id,
            const std::pmr::string& path,
            uint64_t hash,
            std::pair<uint16_t, uint16_t> modified_at)
      : id_(id),
        filepath_(path, &memory::kSpiRamResource),
        tags_hash_(hash),
        is_tombstoned_(false),
        modified_at_(modified_at) {}

  TrackData(TrackId id,
            const std::pmr::string& path,
            uint64_t hash,
            bool is_tombstoned,
            std::pair<uint16_t, uint16_t> modified_at)
      : id_(id),
        filepath_(path, &memory::kSpiRamResource),
        tags_hash_(hash),
        is_tombstoned_(is_tombstoned),
        modified_at_(modified_at) {}

  TrackData(TrackData&& other) = delete;
  TrackData& operator=(TrackData& other) = delete;

  bool operator==(const TrackData&) const = default;

  auto id() const -> TrackId { return id_; }
  auto filepath() const -> std::pmr::string { return filepath_; }
  auto tags_hash() const -> uint64_t { return tags_hash_; }
  auto is_tombstoned() const -> bool { return is_tombstoned_; }
  auto modified_at() const -> std::pair<uint16_t, uint16_t> {
    return modified_at_;
  }

  auto UpdateHash(uint64_t new_hash) const -> TrackData;

  /*
   * Marks this track data as a 'tombstone'. Tombstoned tracks are not playable,
   * and should not generally be shown to users.
   */
  auto Entomb() const -> TrackData;

  /*
   * Clears the tombstone bit of this track, and updates the path to reflect its
   * new location.
   */
  auto Exhume(const std::pmr::string& new_path) const -> TrackData;
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
  std::shared_ptr<TrackData> data_;
  std::shared_ptr<TrackTags> tags_;
};

}  // namespace database
