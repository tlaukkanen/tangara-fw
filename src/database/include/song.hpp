#pragma once

#include <stdint.h>

#include <optional>
#include <string>

#include "leveldb/db.h"
#include "span.hpp"

namespace database {

/*
 * Uniquely describes a single song within the database. This value will be
 * consistent across database updates, and should ideally (but is not guaranteed
 * to) endure even across a song being removed and re-added.
 *
 * Four billion songs should be enough for anybody.
 */
typedef uint32_t SongId;

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
};

/*
 * Owning container for tag-related song metadata that was extracted from a
 * file.
 */
struct SongTags {
  Encoding encoding;
  std::optional<std::string> title;

  // TODO(jacqueline): It would be nice to use shared_ptr's for the artist and
  // album, since there's likely a fair number of duplicates for each
  // (especially the former).

  std::optional<std::string> artist;
  std::optional<std::string> album;

  /*
   * Returns a hash of the 'identifying' tags of this song. That is, a hash that
   * can be used to determine if one song is likely the same as another, across
   * things like re-encoding, re-mastering, or moving the underlying file.
   */
  auto Hash() const -> uint64_t;

  bool operator==(const SongTags&) const = default;
};

/*
 * Immutable owning container for all of the metadata we store for a particular
 * song. This includes two main kinds of metadata:
 *  1. static(ish) attributes, such as the id, path on disk, hash of the tags
 *  2. dynamic attributes, such as the number of times this song has been
 *  played.
 *
 * Because a SongData is immutable, it is thread safe but will not reflect any
 * changes to the dynamic attributes that may happen after it was obtained.
 *
 * Songs may be 'tombstoned'; this indicates that the song is no longer present
 * at its previous location on disk, and we do not have any existing files with
 * a matching tags_hash. When this is the case, we ignore this SongData for most
 * purposes. We keep the entry in our database so that we can properly restore
 * dynamic attributes (such as play count) if the song later re-appears on disk.
 */
class SongData {
 private:
  const SongId id_;
  const std::string filepath_;
  const uint64_t tags_hash_;
  const uint32_t play_count_;
  const bool is_tombstoned_;

 public:
  /* Constructor used when adding new songs to the database. */
  SongData(SongId id, const std::string& path, uint64_t hash)
      : id_(id),
        filepath_(path),
        tags_hash_(hash),
        play_count_(0),
        is_tombstoned_(false) {}

  SongData(SongId id,
           const std::string& path,
           uint64_t hash,
           uint32_t play_count,
           bool is_tombstoned)
      : id_(id),
        filepath_(path),
        tags_hash_(hash),
        play_count_(play_count),
        is_tombstoned_(is_tombstoned) {}

  auto id() const -> SongId { return id_; }
  auto filepath() const -> std::string { return filepath_; }
  auto play_count() const -> uint32_t { return play_count_; }
  auto tags_hash() const -> uint64_t { return tags_hash_; }
  auto is_tombstoned() const -> bool { return is_tombstoned_; }

  auto UpdateHash(uint64_t new_hash) const -> SongData;

  /*
   * Marks this song data as a 'tombstone'. Tombstoned songs are not playable,
   * and should not generally be shown to users.
   */
  auto Entomb() const -> SongData;

  /*
   * Clears the tombstone bit of this song, and updates the path to reflect its
   * new location.
   */
  auto Exhume(const std::string& new_path) const -> SongData;

  bool operator==(const SongData&) const = default;
};

/*
 * Immutable and owning combination of a song's tags and metadata.
 *
 * Note that instances of this class may have a fairly large memory impact, due
 * to the large number of strings they own. Prefer to query the database again
 * (which has its own caching layer), rather than retaining Song instances for a
 * long time.
 */
class Song {
 public:
  Song(SongData data, SongTags tags) : data_(data), tags_(tags) {}

  auto data() -> const SongData& { return data_; }
  auto tags() -> const SongTags& { return tags_; }

  bool operator==(const Song&) const = default;

 private:
  const SongData data_;
  const SongTags tags_;
};

}  // namespace database
