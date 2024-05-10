/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio/track_queue.hpp"
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

#include "audio/audio_events.hpp"
#include "audio/audio_fsm.hpp"
#include "cppbor.h"
#include "cppbor_parse.h"
#include "database/database.hpp"
#include "database/track.hpp"
#include "events/event_queue.hpp"
#include "memory_resource.hpp"
#include "tasks.hpp"
#include "ui/ui_fsm.hpp"

namespace audio {

[[maybe_unused]] static constexpr char kTag[] = "tracks";

using Reason = QueueUpdate::Reason;

RandomIterator::RandomIterator()
    : seed_(0), pos_(0), size_(0), replay_(false) {}

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

auto notifyChanged(bool current_changed, Reason reason) -> void {
  QueueUpdate ev{
      .current_changed = current_changed,
      .reason = reason,
  };
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
    current_changed = was_queue_empty || index == pos_;
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
    notifyChanged(current_changed, Reason::kExplicitUpdate);
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
      notifyChanged(current_changed, Reason::kExplicitUpdate);
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
  next(Reason::kExplicitUpdate);
}

auto TrackQueue::next(Reason r) -> void {
  bool changed = true;

  {
    const std::unique_lock<std::shared_mutex> lock(mutex_);
    if (shuffle_) {
      shuffle_->next();
      pos_ = shuffle_->current();
    } else {
      if (pos_ + 1 >= tracks_.size()) {
        if (replay_) {
          pos_ = 0;
        } else {
          pos_ = tracks_.size();
          changed = false;
        }
      } else {
        pos_++;
      }
    }
  }

  notifyChanged(changed, r);
}

auto TrackQueue::previous() -> void {
  bool changed = true;

  {
    const std::unique_lock<std::shared_mutex> lock(mutex_);
    if (shuffle_) {
      shuffle_->prev();
      pos_ = shuffle_->current();
    } else {
      if (pos_ == 0) {
        if (repeat_) {
          pos_ = tracks_.size() - 1;
        } else {
          changed = false;
        }
      } else {
        pos_--;
      }
    }
  }

  notifyChanged(changed, Reason::kExplicitUpdate);
}

auto TrackQueue::finish() -> void {
  if (repeat_) {
    notifyChanged(true, Reason::kRepeatingLastTrack);
  } else {
    next(Reason::kTrackFinished);
  }
}

auto TrackQueue::skipTo(database::TrackId id) -> void {
  // Defer this work to the background not because it's particularly
  // long-running (although it could be), but because we want to ensure we
  // only search for the given id after any previously pending iterator
  // insertions have finished.
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
      notifyChanged(true, Reason::kExplicitUpdate);
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

  notifyChanged(true, Reason::kExplicitUpdate);
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
  notifyChanged(false, Reason::kExplicitUpdate);
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

  notifyChanged(false, Reason::kExplicitUpdate);
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
  notifyChanged(false, Reason::kExplicitUpdate);
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
  cppbor::Map encoded;
  encoded.add(cppbor::Uint{0}, cppbor::Array{
                                   cppbor::Uint{pos_},
                                   cppbor::Bool{repeat_},
                                   cppbor::Bool{replay_},
                               });
  if (shuffle_) {
    encoded.add(cppbor::Uint{1}, cppbor::Array{
                                     cppbor::Uint{shuffle_->size()},
                                     cppbor::Uint{shuffle_->seed()},
                                     cppbor::Uint{shuffle_->pos()},
                                 });
  }
  encoded.add(cppbor::Uint{2}, std::move(tracks));
  return encoded.toString();
}

TrackQueue::QueueParseClient::QueueParseClient(TrackQueue& queue)
    : queue_(queue), state_(State::kInit), i_(0) {}

cppbor::ParseClient* TrackQueue::QueueParseClient::item(
    std::unique_ptr<cppbor::Item>& item,
    const uint8_t* hdrBegin,
    const uint8_t* valueBegin,
    const uint8_t* end) {
  if (state_ == State::kInit) {
    if (item->type() == cppbor::MAP) {
      state_ = State::kRoot;
    }
  } else if (state_ == State::kRoot) {
    if (item->type() == cppbor::UINT) {
      switch (item->asUint()->unsignedValue()) {
        case 0:
          state_ = State::kMetadata;
          break;
        case 1:
          state_ = State::kShuffle;
          break;
        case 2:
          state_ = State::kTracks;
          break;
        default:
          state_ = State::kFinished;
      }
    }
  } else if (state_ == State::kMetadata) {
    if (item->type() == cppbor::ARRAY) {
      i_ = 0;
    } else if (item->type() == cppbor::UINT) {
      queue_.pos_ = item->asUint()->unsignedValue();
    } else if (item->type() == cppbor::SIMPLE) {
      bool val = item->asBool()->value();
      if (i_ == 0) {
        queue_.repeat_ = val;
      } else if (i_ == 1) {
        queue_.replay_ = val;
      }
      i_++;
    }
  } else if (state_ == State::kShuffle) {
    if (item->type() == cppbor::ARRAY) {
      i_ = 0;
      queue_.shuffle_.emplace();
      queue_.shuffle_->replay(queue_.replay_);
    } else if (item->type() == cppbor::UINT) {
      auto val = item->asUint()->unsignedValue();
      switch (i_) {
        case 0:
          queue_.shuffle_->size() = val;
          break;
        case 1:
          queue_.shuffle_->seed() = val;
          break;
        case 2:
          queue_.shuffle_->pos() = val;
          break;
        default:
          break;
      }
      i_++;
    }
  } else if (state_ == State::kTracks) {
    if (item->type() == cppbor::UINT) {
      queue_.tracks_.push_back(item->asUint()->unsignedValue());
    }
  } else if (state_ == State::kFinished) {
  }
  return this;
}

cppbor::ParseClient* TrackQueue::QueueParseClient::itemEnd(
    std::unique_ptr<cppbor::Item>& item,
    const uint8_t* hdrBegin,
    const uint8_t* valueBegin,
    const uint8_t* end) {
  if (state_ == State::kInit) {
    state_ = State::kFinished;
  } else if (state_ == State::kRoot) {
    state_ = State::kFinished;
  } else if (state_ == State::kMetadata) {
    if (item->type() == cppbor::ARRAY) {
      state_ = State::kRoot;
    }
  } else if (state_ == State::kShuffle) {
    if (item->type() == cppbor::ARRAY) {
      state_ = State::kRoot;
    }
  } else if (state_ == State::kTracks) {
    if (item->type() == cppbor::ARRAY) {
      state_ = State::kRoot;
    }
  } else if (state_ == State::kFinished) {
  }
  return this;
}

auto TrackQueue::deserialise(const std::string& s) -> void {
  if (s.empty()) {
    return;
  }
  QueueParseClient client{*this};
  const uint8_t* data = reinterpret_cast<const uint8_t*>(s.data());
  cppbor::parse(data, data + s.size(), &client);
  notifyChanged(true, Reason::kDeserialised);
}

}  // namespace audio
