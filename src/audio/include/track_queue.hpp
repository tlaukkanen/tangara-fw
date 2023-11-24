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
  auto AddNext(std::shared_ptr<playlist::ISource>) -> void;

  auto IncludeNext(std::shared_ptr<playlist::IResetableSource>) -> void;

  /*
   * Enqueues a track, placing it the end of all enqueued tracks.
   *
   * If there is no current track, the given track will begin playback.
   */
  auto AddLast(database::TrackId) -> void;
  auto AddLast(std::shared_ptr<playlist::ISource>) -> void;

  auto IncludeLast(std::shared_ptr<playlist::IResetableSource>) -> void;

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

  auto Position() -> size_t;
  auto Size() -> size_t;

  TrackQueue(const TrackQueue&) = delete;
  TrackQueue& operator=(const TrackQueue&) = delete;

 private:
  mutable std::mutex mutex_;

  std::list<std::variant<database::TrackId,
                         std::shared_ptr<playlist::IResetableSource>>>
      played_;
  std::list<std::variant<database::TrackId,
                         std::shared_ptr<playlist::ISource>,
                         std::shared_ptr<playlist::IResetableSource>>>
      enqueued_;
};

}  // namespace audio
