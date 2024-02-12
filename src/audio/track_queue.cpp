/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "track_queue.hpp"
#include <stdint.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <variant>

#include "MillerShuffle.h"
#include "esp_random.h"

#include "audio_events.hpp"
#include "audio_fsm.hpp"
#include "cppbor.h"
#include "cppbor_parse.h"
#include "database.hpp"
#include "event_queue.hpp"
#include "memory_resource.hpp"
#include "tasks.hpp"
#include "track.hpp"
#include "ui_fsm.hpp"

namespace audio {

[[maybe_unused]] static constexpr char kTag[] = "tracks";

RandomIterator::RandomIterator(size_t size)
    : seed_(), pos_(0), size_(size), replay_(false) {
  esp_fill_random(&seed_, sizeof(seed_));
}

auto RandomIterator::current() const -> size_t {
  if (pos_ < size_ || replay_) {
    return MillerShuffle(pos_, seed_, size_);
  }
  return size_;
}

auto RandomIterator::next() -> void {
  // MillerShuffle behaves well with pos > size, returning different
  // permutations each 'cycle'. We therefore don't need to worry about wrapping
  // this value.
  pos_++;
}

auto RandomIterator::prev() -> void {
  if (pos_ > 0) {
    pos_--;
  }
}

auto RandomIterator::resize(size_t s) -> void {
  size_ = s;
  // Changing size will yield a different current position anyway, so reset pos
  // to ensure we yield a full sweep of both new and old indexes.
  pos_ = 0;
}

auto RandomIterator::replay(bool r) -> void {
  replay_ = r;
}

auto notifyChanged(bool current_changed) -> void {
  QueueUpdate ev{.current_changed = current_changed};
  events::Ui().Dispatch(ev);
  events::Audio().Dispatch(ev);
}

TrackQueue::TrackQueue(tasks::WorkerPool& bg_worker)
    : mutex_(),
      bg_worker_(bg_worker),
      pos_(0),
      tracks_(&memory::kSpiRamResource),
      shuffle_(),
      repeat_(false),
      replay_(false) {}

auto TrackQueue::current() const -> std::optional<database::TrackId> {
  const std::shared_lock<std::shared_mutex> lock(mutex_);
  if (pos_ >= tracks_.size()) {
    return {};
  }
  return tracks_[pos_];
}

auto TrackQueue::peekNext(std::size_t limit) const
    -> std::vector<database::TrackId> {
  const std::shared_lock<std::shared_mutex> lock(mutex_);
  std::vector<database::TrackId> out;
  for (size_t i = pos_ + 1; i < pos_ + limit + 1 && i < tracks_.size(); i++) {
    out.push_back(i);
  }
  return out;
}

auto TrackQueue::peekPrevious(std::size_t limit) const
    -> std::vector<database::TrackId> {
  const std::shared_lock<std::shared_mutex> lock(mutex_);
  std::vector<database::TrackId> out;
  for (size_t i = pos_ - 1; i < pos_ - limit - 1 && i >= tracks_.size(); i--) {
    out.push_back(i);
  }
  return out;
}

auto TrackQueue::currentPosition() const -> size_t {
  const std::shared_lock<std::shared_mutex> lock(mutex_);
  return pos_;
}

auto TrackQueue::totalSize() const -> size_t {
  const std::shared_lock<std::shared_mutex> lock(mutex_);
  return tracks_.size();
}

auto TrackQueue::insert(Item i, size_t index) -> void {
  bool was_queue_empty;
  bool current_changed;
  {
    const std::shared_lock<std::shared_mutex> lock(mutex_);
    was_queue_empty = pos_ == tracks_.size();
    current_changed = pos_ == was_queue_empty || index == pos_;
  }

  auto update_shuffler = [=, this]() {
    if (shuffle_) {
      shuffle_->resize(tracks_.size());
      // If there wasn't anything already playing, then we should make sure we
      // begin playback at a random point, instead of always starting with
      // whatever was inserted first and *then* shuffling.
      // We don't base this purely off of current_changed because we would like
      // 'play this track now' (by inserting at the current pos) to work even
      // when shuffling is enabled.
      if (was_queue_empty) {
        pos_ = shuffle_->current();
      }
    }
  };

  if (std::holds_alternative<database::TrackId>(i)) {
    {
      const std::unique_lock<std::shared_mutex> lock(mutex_);
      if (index <= tracks_.size()) {
        tracks_.insert(tracks_.begin() + index, std::get<database::TrackId>(i));
        update_shuffler();
      }
    }
    notifyChanged(current_changed);
  } else if (std::holds_alternative<database::TrackIterator>(i)) {
    // Iterators can be very large, and retrieving items from them often
    // requires disk i/o. Handle them asynchronously so that inserting them
    // doesn't block.
    bg_worker_.Dispatch<void>([=, this]() {
      database::TrackIterator it = std::get<database::TrackIterator>(i);
      size_t working_pos = index;
      while (true) {
        auto next = *it;
        if (!next) {
          break;
        }
        // Keep this critical section small so that we're not blocking methods
        // like current().
        {
          const std::unique_lock<std::shared_mutex> lock(mutex_);
          if (working_pos <= tracks_.size()) {
            tracks_.insert(tracks_.begin() + working_pos, *next);
          }
        }
        working_pos++;
        it++;
      }
      {
        const std::unique_lock<std::shared_mutex> lock(mutex_);
        update_shuffler();
      }
      notifyChanged(current_changed);
    });
  }
}

auto TrackQueue::append(Item i) -> void {
  size_t end;
  {
    const std::shared_lock<std::shared_mutex> lock(mutex_);
    end = tracks_.size();
  }
  insert(i, end);
}

auto TrackQueue::next() -> void {
  const std::unique_lock<std::shared_mutex> lock(mutex_);
  if (shuffle_) {
    shuffle_->next();
    pos_ = shuffle_->current();
  } else {
    if (pos_ + 1 >= tracks_.size()) {
      if (replay_) {
        pos_ = 0;
      }
    } else {
      pos_++;
    }
  }

  notifyChanged(true);
}

auto TrackQueue::previous() -> void {
  const std::unique_lock<std::shared_mutex> lock(mutex_);
  if (shuffle_) {
    shuffle_->prev();
    pos_ = shuffle_->current();
  } else {
    if (pos_ == 0) {
      if (repeat_) {
        pos_ = tracks_.size() - 1;
      }
    } else {
      pos_--;
    }
  }

  notifyChanged(true);
}

auto TrackQueue::finish() -> void {
  if (repeat_) {
    notifyChanged(true);
  } else {
    next();
  }
}

auto TrackQueue::skipTo(database::TrackId id) -> void {
  // Defer this work to the background not because it's particularly
  // long-running (although it could be), but because we want to ensure we only
  // search for the given id after any previously pending iterator insertions
  // have finished.
  bg_worker_.Dispatch<void>([=, this]() {
    bool found = false;
    {
      const std::unique_lock<std::shared_mutex> lock(mutex_);
      for (size_t i = 0; i < tracks_.size(); i++) {
        if (tracks_[i] == id) {
          pos_ = i;
          found = true;
          break;
        }
      }
    }
    if (found) {
      notifyChanged(true);
    }
  });
}

auto TrackQueue::clear() -> void {
  {
    const std::unique_lock<std::shared_mutex> lock(mutex_);
    if (tracks_.empty()) {
      return;
    }

    pos_ = 0;
    tracks_.clear();

    if (shuffle_) {
      shuffle_->resize(0);
    }
  }

  notifyChanged(true);
}

auto TrackQueue::random(bool en) -> void {
  {
    const std::unique_lock<std::shared_mutex> lock(mutex_);
    // Don't check for en == true already; this has the side effect that
    // repeated calls with en == true will re-shuffle.
    if (en) {
      shuffle_.emplace(tracks_.size());
      shuffle_->replay(replay_);
    } else {
      shuffle_.reset();
    }
  }

  // Current track doesn't get randomised until next().
  notifyChanged(false);
}

auto TrackQueue::random() const -> bool {
  const std::shared_lock<std::shared_mutex> lock(mutex_);
  return shuffle_.has_value();
}

auto TrackQueue::repeat(bool en) -> void {
  {
    const std::unique_lock<std::shared_mutex> lock(mutex_);
    repeat_ = en;
  }

  notifyChanged(false);
}

auto TrackQueue::repeat() const -> bool {
  const std::shared_lock<std::shared_mutex> lock(mutex_);
  return repeat_;
}

auto TrackQueue::replay(bool en) -> void {
  {
    const std::unique_lock<std::shared_mutex> lock(mutex_);
    replay_ = en;
    if (shuffle_) {
      shuffle_->replay(en);
    }
  }
  notifyChanged(false);
}

auto TrackQueue::replay() const -> bool {
  const std::shared_lock<std::shared_mutex> lock(mutex_);
  return replay_;
}

auto TrackQueue::serialise() -> std::string {
  cppbor::Array tracks{};
  for (database::TrackId track : tracks_) {
    tracks.add(cppbor::Uint(track));
  }
  // FIXME: this should include the RandomIterator's seed as well.
  cppbor::Array encoded{
      cppbor::Uint{pos_},
      std::move(tracks),
  };
  return encoded.toString();
}

class QueueParseClient : public cppbor::ParseClient {
 public:
  QueueParseClient(size_t& pos, std::pmr::vector<database::TrackId>& tracks)
      : pos_(pos),
        tracks_(tracks),
        in_root_array_(false),
        in_track_list_(false) {}

  ParseClient* item(std::unique_ptr<cppbor::Item>& item,
                    const uint8_t* hdrBegin,
                    const uint8_t* valueBegin,
                    const uint8_t* end) override {
    if (item->type() == cppbor::ARRAY) {
      if (!in_root_array_) {
        in_root_array_ = true;
      } else {
        in_track_list_ = true;
      }
    } else if (item->type() == cppbor::UINT) {
      auto val = item->asUint()->unsignedValue();
      if (in_track_list_) {
        tracks_.push_back(val);
      } else {
        pos_ = static_cast<size_t>(val);
      }
    }
    return this;
  }

  ParseClient* itemEnd(std::unique_ptr<cppbor::Item>& item,
                       const uint8_t* hdrBegin,
                       const uint8_t* valueBegin,
                       const uint8_t* end) override {
    return this;
  }

  void error(const uint8_t* position,
             const std::string& errorMessage) override {}

 private:
  size_t& pos_;
  std::pmr::vector<database::TrackId>& tracks_;

  bool in_root_array_;
  bool in_track_list_;
};

auto TrackQueue::deserialise(const std::string& s) -> void {
  if (s.empty()) {
    return;
  }
  QueueParseClient client{pos_, tracks_};
  const uint8_t* data = reinterpret_cast<const uint8_t*>(s.data());
  cppbor::parse(data, data + s.size(), &client);
  notifyChanged(true);
}

}  // namespace audio
