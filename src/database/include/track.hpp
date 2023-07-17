/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "leveldb/db.h"
#include "shared_string.h"
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
enum class Encoding {
  kUnsupported = 0,
  kMp3 = 1,
  kWav = 2,
  kOgg = 3,
  kFlac = 4,
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
  auto encoding() const -> Encoding { return encoding_; };
  auto encoding(Encoding e) -> void { encoding_ = e; };

  TrackTags() : encoding_(Encoding::kUnsupported) {}

  std::optional<int> channels;
  std::optional<int> sample_rate;
  std::optional<int> bits_per_sample;

  std::optional<int> duration;

  auto set(const Tag& key, const std::string& val) -> void;
  auto at(const Tag& key) const -> std::optional<shared_string>;
  auto operator[](const Tag& key) const -> std::optional<shared_string>;

  /*
   * Returns a hash of the 'identifying' tags of this track. That is, a hash
   * that can be used to determine if one track is likely the same as another,
   * across things like re-encoding, re-mastering, or moving the underlying
   * file.
   */
  auto Hash() const -> uint64_t;

  bool operator==(const TrackTags&) const = default;
  TrackTags& operator=(const TrackTags&) = default;
  TrackTags(const TrackTags&) = default;

 private:
  Encoding encoding_;
  std::unordered_map<Tag, shared_string> tags_;
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
  const std::string filepath_;
  const uint64_t tags_hash_;
  const uint32_t play_count_;
  const bool is_tombstoned_;

 public:
  /* Constructor used when adding new tracks to the database. */
  TrackData(TrackId id, const std::string& path, uint64_t hash)
      : id_(id),
        filepath_(path),
        tags_hash_(hash),
        play_count_(0),
        is_tombstoned_(false) {}

  TrackData(TrackId id,
            const std::string& path,
            uint64_t hash,
            uint32_t play_count,
            bool is_tombstoned)
      : id_(id),
        filepath_(path),
        tags_hash_(hash),
        play_count_(play_count),
        is_tombstoned_(is_tombstoned) {}

  auto id() const -> TrackId { return id_; }
  auto filepath() const -> std::string { return filepath_; }
  auto play_count() const -> uint32_t { return play_count_; }
  auto tags_hash() const -> uint64_t { return tags_hash_; }
  auto is_tombstoned() const -> bool { return is_tombstoned_; }

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
  auto Exhume(const std::string& new_path) const -> TrackData;

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
  Track(const TrackData& data, const TrackTags& tags)
      : data_(data), tags_(tags) {}
  Track(const Track& other) = default;

  auto data() const -> const TrackData& { return data_; }
  auto tags() const -> const TrackTags& { return tags_; }

  auto TitleOrFilename() const -> shared_string;

  bool operator==(const Track&) const = default;
  Track operator=(const Track& other) const { return Track(other); }

 private:
  const TrackData data_;
  const TrackTags tags_;
};

void swap(Track& first, Track& second);

}  // namespace database
