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
#include "random.hpp"
#include "tasks.hpp"
#include "track_queue.hpp"
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

auto notifyPlayFrom(uint32_t start_from_position) -> void {
  QueueUpdate ev{
      .current_changed = true,
      .reason = Reason::kExplicitUpdate,
      .seek_to_second = start_from_position,
  };
  events::Ui().Dispatch(ev);
  events::Audio().Dispatch(ev);
}

TrackQueue::TrackQueue(tasks::WorkerPool& bg_worker, database::Handle db)
    : mutex_(),
      bg_worker_(bg_worker),
      db_(db),
      playlist_(".queue.playlist"),
      position_(0),
      shuffle_(),
      repeat_(false),
      replay_(false) {}

auto TrackQueue::current() const -> TrackItem {
  const std::shared_lock<std::shared_mutex> lock(mutex_);
  std::string val;
  if (opened_playlist_ && position_ < opened_playlist_->size()) {
    val = opened_playlist_->value();
  } else {
    val = playlist_.value();
  }
  if (val.empty()) {
    return {};
  }
  return val;
}

auto TrackQueue::playFromPosition(const std::string& filepath,
                                  uint32_t position) -> void {
  clear();
  {
    const std::unique_lock<std::shared_mutex> lock(mutex_);
    playlist_.append(filepath);
    updateShuffler(true);
  }
  notifyPlayFrom(position);
}

auto TrackQueue::currentPosition() const -> size_t {
  const std::shared_lock<std::shared_mutex> lock(mutex_);
  return position_;
}

auto TrackQueue::totalSize() const -> size_t {
  size_t sum = playlist_.size();
  if (opened_playlist_) {
    sum += opened_playlist_->size();
  }
  return sum;
}

auto TrackQueue::updateShuffler(bool andUpdatePosition) -> void {
  if (shuffle_) {
    shuffle_->resize(totalSize());
    if (andUpdatePosition) {
      goTo(shuffle_->current());
    }
  }
}

auto TrackQueue::open() -> bool {
  // FIX ME: If playlist opening fails, should probably fall back to a vector of
  // tracks or something so that we're not necessarily always needing mounted
  // storage
  return playlist_.open();
}

auto TrackQueue::close() -> void {
  playlist_.close();
  if (opened_playlist_) {
    opened_playlist_->close();
  }
}

auto TrackQueue::openPlaylist(const std::string& playlist_file, bool notify)
    -> bool {
  opened_playlist_.emplace(playlist_file);
  auto res = opened_playlist_->open();
  if (!res) {
    return false;
  }
  updateShuffler(true);
  if (notify) {
    notifyChanged(true, Reason::kExplicitUpdate);
  }
  return true;
}

auto TrackQueue::getFilepath(database::TrackId id)
    -> std::optional<std::string> {
  auto db = db_.lock();
  if (!db) {
    return {};
  }
  return db->getTrackPath(id);
}

auto TrackQueue::append(Item i) -> void {
  bool was_queue_empty;
  bool current_changed;
  {
    const std::shared_lock<std::shared_mutex> lock(mutex_);
    was_queue_empty = playlist_.currentPosition() >= playlist_.size();
    current_changed = was_queue_empty;  // Dont support inserts yet
  }

  if (std::holds_alternative<database::TrackId>(i)) {
    {
      const std::unique_lock<std::shared_mutex> lock(mutex_);
      auto filename = getFilepath(std::get<database::TrackId>(i)).value_or("");
      if (!filename.empty()) {
        playlist_.append(filename);
      }
      updateShuffler(was_queue_empty);
    }
    notifyChanged(current_changed, Reason::kExplicitUpdate);
  } else if (std::holds_alternative<std::string>(i)) {
    auto& path = std::get<std::string>(i);
    if (!path.empty()) {
      {
        const std::unique_lock<std::shared_mutex> lock(mutex_);
        playlist_.append(std::get<std::string>(i));
        updateShuffler(was_queue_empty);
      }
      notifyChanged(current_changed, Reason::kExplicitUpdate);
    }
  } else if (std::holds_alternative<database::TrackIterator>(i)) {
    // Iterators can be very large, and retrieving items from them often
    // requires disk i/o. Handle them asynchronously so that inserting them
    // doesn't block.
    bg_worker_.Dispatch<void>([=, this]() {
      database::TrackIterator it = std::get<database::TrackIterator>(i);

      size_t next_update_at = 10;
      while (true) {
        auto next = *it;
        if (!next) {
          break;
        }
        // Keep this critical section small so that we're not blocking methods
        // like current().
        {
          const std::unique_lock<std::shared_mutex> lock(mutex_);
          auto filename = getFilepath(*next).value_or("");
          if (!filename.empty()) {
            playlist_.append(filename);
          }
        }
        it++;

        // Appending very large iterators can take a while. Send out periodic
        // queue updates during them so that the user has an idea what's going
        // on.
        if (!--next_update_at) {
          next_update_at = util::sRandom->RangeInclusive(10, 20);
          notifyChanged(false, Reason::kBulkLoadingUpdate);
        }
      }

      {
        const std::unique_lock<std::shared_mutex> lock(mutex_);
        updateShuffler(was_queue_empty);
      }
      notifyChanged(current_changed, Reason::kExplicitUpdate);
    });
  }
}

auto TrackQueue::next() -> void {
  next(Reason::kExplicitUpdate);
}

auto TrackQueue::currentPosition(size_t position) -> bool {
  {
    const std::shared_lock<std::shared_mutex> lock(mutex_);
    if (position >= totalSize()) {
      return false;
    }
    goTo(position);
  }

  // If we're explicitly setting the position, we want to treat it as though
  // the current track has changed, even if the position was the same
  notifyChanged(true, Reason::kExplicitUpdate);
  return true;
}

auto TrackQueue::goTo(size_t position) -> void {
  position_ = position;
  if (opened_playlist_) {
    if (position_ < opened_playlist_->size()) {
      opened_playlist_->skipTo(position_);
    } else {
      playlist_.skipTo(position_ - opened_playlist_->size());
    }
  } else {
    playlist_.skipTo(position_);
  }
}

auto TrackQueue::next(Reason r) -> void {
  bool changed = true;

  {
    const std::unique_lock<std::shared_mutex> lock(mutex_);
    auto pos = position_;

    if (shuffle_) {
      shuffle_->next();
      position_ = shuffle_->current();
    } else {
      if (position_ + 1 < totalSize()) {
        position_++;
      }
    }

    goTo(position_);
    changed = pos != position_;
  }

  notifyChanged(changed, r);
}

auto TrackQueue::previous() -> void {
  bool changed = true;

  {
    const std::unique_lock<std::shared_mutex> lock(mutex_);
    if (shuffle_) {
      shuffle_->prev();
      position_ = shuffle_->current();
    } else {
      if (position_ > 0) {
        position_--;
      }
    }
    goTo(position_);
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

auto TrackQueue::clear() -> void {
  {
    const std::unique_lock<std::shared_mutex> lock(mutex_);
    position_ = 0;
    playlist_.clear();
    opened_playlist_.reset();
    if (shuffle_) {
      shuffle_->resize(0);
    }
  }

  notifyChanged(true, Reason::kExplicitUpdate);
}

auto TrackQueue::random(bool en) -> void {
  {
    const std::unique_lock<std::shared_mutex> lock(mutex_);
    if (en) {
      shuffle_.emplace(totalSize());
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
  cppbor::Map encoded;

  cppbor::Array metadata{
      cppbor::Bool{repeat_},
      cppbor::Bool{replay_},
      cppbor::Uint{position_},
  };

  if (opened_playlist_) {
    metadata.add(cppbor::Tstr{opened_playlist_->filepath()});
  }

  encoded.add(cppbor::Uint{0}, std::move(metadata));

  if (shuffle_) {
    encoded.add(cppbor::Uint{1}, cppbor::Array{
                                     cppbor::Uint{shuffle_->size()},
                                     cppbor::Uint{shuffle_->seed()},
                                     cppbor::Uint{shuffle_->pos()},
                                 });
  }

  playlist_.serialiseCache();

  return encoded.toString();
}

TrackQueue::QueueParseClient::QueueParseClient(TrackQueue& queue)
    : queue_(queue), state_(State::kInit), i_(0), position_to_set_(0) {}

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
        default:
          state_ = State::kFinished;
      }
    }
  } else if (state_ == State::kMetadata) {
    if (item->type() == cppbor::ARRAY) {
      i_ = 0;
    } else if (item->type() == cppbor::UINT) {
      auto val = item->asUint()->unsignedValue();
      // Save the position so we can apply it later when we have finished
      // serialising
      position_to_set_ = val;
    } else if (item->type() == cppbor::TSTR) {
      auto val = item->asTstr();
      queue_.openPlaylist(val->value(), false);
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
    queue_.goTo(position_to_set_);
    state_ = State::kFinished;
  } else if (state_ == State::kMetadata) {
    if (item->type() == cppbor::ARRAY) {
      state_ = State::kRoot;
    }
  } else if (state_ == State::kShuffle) {
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
