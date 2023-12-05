/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <vector>

#include "database.hpp"
#include "source.hpp"
#include "track.hpp"

namespace audio {

/*
 * Owns and manages a complete view of the playback queue. Includes the
 * currently playing track, a truncated list of previously played tracks, and
 * all future tracks that have been queued.
 *
 * In order to not use all of our memory, this class deals strictly with track
 * ids. Consumers that need more data than this should fetch it from the
 * database.
 *
 * Instances of this class are broadly safe to use from multiple tasks; each
 * method represents an atomic operation. No guarantees are made about
 * consistency between calls however.
 */
class TrackQueue {
 public:
  TrackQueue();

  class Editor {
   public:
    ~Editor();

    // Cannot be copied or moved.
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

   private:
    friend TrackQueue;

    Editor(TrackQueue&);

    std::lock_guard<std::recursive_mutex> lock_;
    bool has_current_changed_;
  };

  auto Edit() -> Editor;

  /* Returns the currently playing track. */
  auto Current() const -> std::optional<database::TrackId>;

  /* Returns, in order, tracks that have been queued to be played next. */
  auto PeekNext(std::size_t limit) const -> std::vector<database::TrackId>;

  /*
   * Returns the tracks in the queue that have already been played, ordered
   * most recently played first.
   */
  auto PeekPrevious(std::size_t limit) const -> std::vector<database::TrackId>;

  auto GetCurrentPosition() const -> size_t;
  auto GetTotalSize() const -> size_t;

  using Item = std::variant<database::TrackId, database::TrackIterator>;
  auto Insert(Editor&, Item, size_t) -> void;
  auto Append(Editor&, Item i) -> void;

  /*
   * Advances to the next track in the queue, placing the current track at the
   * front of the 'played' queue.
   */
  auto Next(Editor&) -> std::optional<database::TrackId>;
  auto Previous(Editor&) -> std::optional<database::TrackId>;

  auto SkipTo(Editor&, database::TrackId) -> void;

  /*
   * Removes all tracks from all queues, and stops any currently playing track.
   */
  auto Clear(Editor&) -> void;

  auto Save(std::weak_ptr<database::Database>) -> void;
  auto Load(std::weak_ptr<database::Database>) -> void;

  // Cannot be copied or moved.
  TrackQueue(const TrackQueue&) = delete;
  TrackQueue& operator=(const TrackQueue&) = delete;

 private:
  // FIXME: Make this a shared_mutex so that multithread reads don't block.
  mutable std::recursive_mutex mutex_;

  std::optional<database::TrackId> current_;

  // Note: stored in reverse order, i.e. most recent played it at the *back* of
  // this vector.
  std::pmr::vector<database::TrackId> played_;
  std::pmr::vector<Item> enqueued_;
};

}  // namespace audio
