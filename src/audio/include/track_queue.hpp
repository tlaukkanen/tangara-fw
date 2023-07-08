/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <deque>
#include <mutex>
#include <vector>

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
 * consistency between calls however. For example, there may be data changes
 * between consecutive calls to AddNext() and GetUpcoming();
 */
class TrackQueue {
 public:
  TrackQueue();

  /* Returns the currently playing track. */
  auto GetCurrent() const -> std::optional<database::TrackId>;
  /* Returns, in order, tracks that have been queued to be played next. */
  auto GetUpcoming(std::size_t limit) const -> std::vector<database::TrackId>;

  /*
   * Enqueues a track, placing it immediately after the current track and
   * before anything already queued.
   *
   * If there is no current track, the given track will begin playback.
   */
  auto AddNext(database::TrackId) -> void;
  auto AddNext(const std::vector<database::TrackId>&) -> void;

  /*
   * Enqueues a track, placing it the end of all enqueued tracks.
   *
   * If there is no current track, the given track will begin playback.
   */
  auto AddLast(database::TrackId) -> void;
  auto AddLast(const std::vector<database::TrackId>&) -> void;

  /*
   * Advances to the next track in the queue, placing the current track at the
   * front of the 'played' queue.
   */
  auto Next() -> void;
  auto Previous() -> void;

  /*
   * Removes all tracks from all queues, and stops any currently playing track.
   */
  auto Clear() -> void;
  /*
   * Removes a specific track from the queue of upcoming tracks. Has no effect
   * on the currently playing track.
   */
  auto RemoveUpcoming(database::TrackId) -> void;

  TrackQueue(const TrackQueue&) = delete;
  TrackQueue& operator=(const TrackQueue&) = delete;

 private:
  mutable std::mutex mutex_;

  std::deque<database::TrackId> played_;
  std::deque<database::TrackId> upcoming_;
  std::optional<database::TrackId> current_;
};

}  // namespace audio
