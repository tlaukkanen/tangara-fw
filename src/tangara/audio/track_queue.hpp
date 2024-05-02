/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

#include "audio_events.hpp"
#include "cppbor_parse.h"
#include "database.hpp"
#include "tasks.hpp"
#include "track.hpp"

namespace audio {

/*
 * Utility that uses a Miller shuffle to yield well-distributed random indexes
 * from within a range.
 */
class RandomIterator {
 public:
  RandomIterator();
  RandomIterator(size_t size);

  auto current() const -> size_t;

  auto next() -> void;
  auto prev() -> void;

  // Note resizing has the side-effect of restarting iteration.
  auto resize(size_t) -> void;
  auto replay(bool) -> void;

  auto seed() -> size_t& { return seed_; }
  auto pos() -> size_t& { return pos_; }
  auto size() -> size_t& { return size_; }

 private:
  size_t seed_;
  size_t pos_;
  size_t size_;
  bool replay_;
};

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
  TrackQueue(tasks::WorkerPool& bg_worker);

  /* Returns the currently playing track. */
  auto current() const -> std::optional<database::TrackId>;

  /* Returns, in order, tracks that have been queued to be played next. */
  auto peekNext(std::size_t limit) const -> std::vector<database::TrackId>;

  /*
   * Returns the tracks in the queue that have already been played, ordered
   * most recently played first.
   */
  auto peekPrevious(std::size_t limit) const -> std::vector<database::TrackId>;

  auto currentPosition() const -> size_t;
  auto totalSize() const -> size_t;

  using Item = std::variant<database::TrackId, database::TrackIterator>;
  auto insert(Item, size_t index = 0) -> void;
  auto append(Item i) -> void;

  /*
   * Advances to the next track in the queue, placing the current track at the
   * front of the 'played' queue.
   */
  auto next() -> void;
  auto previous() -> void;

  /*
   * Called when the current track finishes
   */
  auto finish() -> void;

  auto skipTo(database::TrackId) -> void;

  /*
   * Removes all tracks from all queues, and stops any currently playing track.
   */
  auto clear() -> void;

  auto random(bool) -> void;
  auto random() const -> bool;

  auto repeat(bool) -> void;
  auto repeat() const -> bool;

  auto replay(bool) -> void;
  auto replay() const -> bool;

  auto serialise() -> std::string;
  auto deserialise(const std::string&) -> void;

  // Cannot be copied or moved.
  TrackQueue(const TrackQueue&) = delete;
  TrackQueue& operator=(const TrackQueue&) = delete;

 private:
  auto next(QueueUpdate::Reason r) -> void;

  mutable std::shared_mutex mutex_;

  tasks::WorkerPool& bg_worker_;

  size_t pos_;
  std::pmr::vector<database::TrackId> tracks_;

  std::optional<RandomIterator> shuffle_;
  bool repeat_;
  bool replay_;

  class QueueParseClient : public cppbor::ParseClient {
   public:
    QueueParseClient(TrackQueue& queue);

    ParseClient* item(std::unique_ptr<cppbor::Item>& item,
                      const uint8_t* hdrBegin,
                      const uint8_t* valueBegin,
                      const uint8_t* end) override;

    ParseClient* itemEnd(std::unique_ptr<cppbor::Item>& item,
                         const uint8_t* hdrBegin,
                         const uint8_t* valueBegin,
                         const uint8_t* end) override;

    void error(const uint8_t* position,
               const std::string& errorMessage) override {}

   private:
    TrackQueue& queue_;

    enum class State {
      kInit,
      kRoot,
      kMetadata,
      kShuffle,
      kTracks,
      kFinished,
    };
    State state_;
    size_t i_;
  };
};

}  // namespace audio
