/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "track_queue.hpp"
#include <stdint.h>

#include <algorithm>
#include <mutex>
#include <optional>
#include <variant>

#include "audio_events.hpp"
#include "audio_fsm.hpp"
#include "cppbor.h"
#include "cppbor_parse.h"
#include "database.hpp"
#include "event_queue.hpp"
#include "memory_resource.hpp"
#include "source.hpp"
#include "track.hpp"
#include "ui_fsm.hpp"

namespace audio {

[[maybe_unused]] static constexpr char kTag[] = "tracks";

static const std::string kSerialiseKey = "queue";
static const std::string kCurrentKey = "cur";
static const std::string kPlayedKey = "prev";
static const std::string kEnqueuedKey = "next";

TrackQueue::Editor::Editor(TrackQueue& queue)
    : lock_(queue.mutex_), has_current_changed_(false) {}

TrackQueue::Editor::~Editor() {
  QueueUpdate ev{.current_changed = has_current_changed_};
  events::Audio().Dispatch(ev);
  events::Ui().Dispatch(ev);
}

TrackQueue::TrackQueue()
    : mutex_(),
      current_(),
      played_(&memory::kSpiRamResource),
      enqueued_(&memory::kSpiRamResource) {}

auto TrackQueue::Edit() -> Editor {
  return Editor(*this);
}

auto TrackQueue::Current() const -> std::optional<database::TrackId> {
  const std::lock_guard<std::recursive_mutex> lock(mutex_);
  return current_;
}

auto TrackQueue::PeekNext(std::size_t limit) const
    -> std::vector<database::TrackId> {
  const std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::vector<database::TrackId> ret;

  for (auto it = enqueued_.begin(); it != enqueued_.end() && limit > 0; it++) {
    std::visit(
        [&](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, database::TrackId>) {
            ret.push_back(arg);
            limit--;
          } else if constexpr (std::is_same_v<T, database::TrackIterator>) {
            auto copy = arg;
            while (limit > 0) {
              auto next = copy.Next();
              if (!next) {
                break;
              }
              ret.push_back(*next);
              limit--;
            }
          }
        },
        *it);
  }

  return ret;
}

auto TrackQueue::PeekPrevious(std::size_t limit) const
    -> std::vector<database::TrackId> {
  const std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::vector<database::TrackId> ret;
  ret.reserve(limit);

  for (auto it = played_.rbegin(); it != played_.rend(); it++, limit--) {
    ret.push_back(*it);
  }

  return ret;
}

auto TrackQueue::GetCurrentPosition() const -> size_t {
  const std::lock_guard<std::recursive_mutex> lock(mutex_);
  size_t played = played_.size();
  if (current_) {
    played += 1;
  }
  return played;
}

auto TrackQueue::GetTotalSize() const -> size_t {
  const std::lock_guard<std::recursive_mutex> lock(mutex_);
  size_t total = GetCurrentPosition();

  for (const auto& item : enqueued_) {
    std::visit(
        [&](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, database::TrackId>) {
            total++;
          } else if constexpr (std::is_same_v<T, database::TrackIterator>) {
            total += arg.Size();
          }
        },
        item);
  }

  return total;
}

auto TrackQueue::Insert(Editor& ed, Item i, size_t index) -> void {
  if (index == 0) {
    enqueued_.insert(enqueued_.begin(), i);
  }

  // We can't insert halfway through an iterator, so we need to ensure that the
  // first `index` items in the queue are reified into track ids.
  size_t current_index = 0;
  while (current_index < index && current_index < enqueued_.size()) {
    std::visit(
        [&](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, database::TrackId>) {
            // This item is already a track id; nothing to do.
            current_index++;
          } else if constexpr (std::is_same_v<T, database::TrackIterator>) {
            // This item is an iterator. Push it back one, replacing its old
            // index with the next value from it.
            auto next = arg.Next();
            auto iterator_index = enqueued_.begin() + current_index;
            if (!next) {
              // Out of values. Remove the iterator completely.
              enqueued_.erase(iterator_index);
              // Don't increment current_index, since the next item in the
              // queue will have been moved down.
            } else {
              enqueued_.insert(iterator_index, *next);
              current_index++;
            }
          }
        },
        enqueued_[current_index]);
  }

  // Double check the previous loop didn't run out of items.
  if (index > enqueued_.size()) {
    ESP_LOGE(kTag, "insert index was out of bounds");
    return;
  }

  // Finally, we can now do the actual insertion.
  enqueued_.insert(enqueued_.begin() + index, i);
}

auto TrackQueue::Append(Editor& ed, Item i) -> void {
  enqueued_.push_back(i);
  if (!current_) {
    Next(ed);
  }
}

auto TrackQueue::Next(Editor& ed) -> std::optional<database::TrackId> {
  if (current_) {
    ed.has_current_changed_ = true;
    played_.push_back(*current_);
  }
  current_.reset();

  while (!current_ && !enqueued_.empty()) {
    ed.has_current_changed_ = true;
    std::visit(
        [&](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, database::TrackId>) {
            current_ = arg;
            enqueued_.erase(enqueued_.begin());
          } else if constexpr (std::is_same_v<T, database::TrackIterator>) {
            auto next = arg.Next();
            if (!next) {
              enqueued_.erase(enqueued_.begin());
            } else {
              current_ = *next;
            }
          }
        },
        enqueued_.front());
  }

  return current_;
}

auto TrackQueue::Previous(Editor& ed) -> std::optional<database::TrackId> {
  if (played_.empty()) {
    return current_;
  }
  ed.has_current_changed_ = true;
  if (current_) {
    enqueued_.insert(enqueued_.begin(), *current_);
  }
  current_ = played_.back();
  played_.pop_back();
  return current_;
}

auto TrackQueue::SkipTo(Editor& ed, database::TrackId id) -> void {
  while ((!current_ || *current_ != id) && !enqueued_.empty()) {
    Next(ed);
  }
}

auto TrackQueue::Clear(Editor& ed) -> void {
  ed.has_current_changed_ = current_.has_value();
  current_.reset();
  played_.clear();
  enqueued_.clear();
}

auto TrackQueue::Save(std::weak_ptr<database::Database> db) -> void {
  cppbor::Map root{};

  if (current_) {
    root.add(cppbor::Bstr{kCurrentKey}, cppbor::Uint{*current_});
  }

  cppbor::Array played{};
  for (const auto& id : played_) {
    played.add(cppbor::Uint{id});
  }
  root.add(cppbor::Bstr{kPlayedKey}, std::move(played));

  cppbor::Array enqueued{};
  for (const auto& item : enqueued_) {
    std::visit(
        [&](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;
          if constexpr (std::is_same_v<T, database::TrackId>) {
            enqueued.add(cppbor::Uint{arg});
          } else if constexpr (std::is_same_v<T, database::TrackIterator>) {
            enqueued.add(arg.cbor());
          }
        },
        item);
  }
  root.add(cppbor::Bstr{kEnqueuedKey}, std::move(enqueued));

  auto db_lock = db.lock();
  if (!db_lock) {
    return;
  }
  db_lock->Put(kSerialiseKey, root.toString());
}

class Parser : public cppbor::ParseClient {
 public:
  Parser(std::weak_ptr<database::Database> db,
         std::optional<database::TrackId>& current,
         std::pmr::vector<database::TrackId>& played,
         std::pmr::vector<TrackQueue::Item>& enqueued)
      : state_(State::kInit),
        db_(db),
        current_(current),
        played_(played),
        enqueued_(enqueued) {}

  virtual ParseClient* item(std::unique_ptr<cppbor::Item>& item,
                            const uint8_t* hdrBegin,
                            const uint8_t* valueBegin,
                            const uint8_t* end) override {
    switch (state_) {
      case State::kInit:
        if (item->type() == cppbor::MAP) {
          state_ = State::kRoot;
        }
        break;
      case State::kRoot:
        if (item->type() != cppbor::TSTR) {
          break;
        }
        if (item->asTstr()->value() == kCurrentKey) {
          state_ = State::kCurrent;
        } else if (item->asTstr()->value() == kPlayedKey) {
          state_ = State::kPlayed;
        } else if (item->asTstr()->value() == kEnqueuedKey) {
          state_ = State::kEnqueued;
        }
        break;
      case State::kCurrent:
        if (item->type() == cppbor::UINT) {
          current_ = item->asUint()->value();
        }
        state_ = State::kRoot;
        break;
      case State::kPlayed:
        if (item->type() == cppbor::UINT) {
          played_.push_back(item->asUint()->value());
        }
        break;
      case State::kEnqueued:
        if (item->type() == cppbor::UINT) {
          played_.push_back(item->asUint()->value());
        } else if (item->type() == cppbor::ARRAY) {
          queue_depth_ = 1;
          state_ = State::kEnqueuedIterator;
        }
        break;
      case State::kEnqueuedIterator:
        if (item->type() == cppbor::MAP || item->type() == cppbor::ARRAY) {
          queue_depth_++;
        }
        break;
      case State::kFinished:
        break;
    }

    return this;
  }

  ParseClient* itemEnd(std::unique_ptr<cppbor::Item>& item,
                       const uint8_t* hdrBegin,
                       const uint8_t* valueBegin,
                       const uint8_t* end) override {
    std::optional<database::TrackIterator> parsed_it;
    switch (state_) {
      case State::kInit:
      case State::kRoot:
      case State::kCurrent:
        state_ = State::kFinished;
        break;
      case State::kEnqueued:
      case State::kPlayed:
        state_ = State::kRoot;
        break;
      case State::kEnqueuedIterator:
        if (item->type() == cppbor::MAP || item->type() == cppbor::ARRAY) {
          queue_depth_++;
        }
        if (queue_depth_ == 0) {
          parsed_it = database::TrackIterator::Parse(db_, *item->asArray());
          if (parsed_it) {
            enqueued_.push_back(std::move(*parsed_it));
          }
        }
        state_ = State::kEnqueued;
        break;
      case State::kFinished:
        break;
    }
    return this;
  }

  void error(const uint8_t* position,
             const std::string& errorMessage) override {
    ESP_LOGE(kTag, "restoring saved queue failed: %s", errorMessage.c_str());
  }

 private:
  enum class State {
    kInit,
    kRoot,
    kCurrent,
    kPlayed,
    kEnqueued,
    kEnqueuedIterator,
    kFinished,
  } state_;

  std::weak_ptr<database::Database> db_;

  int queue_depth_;

  std::optional<database::TrackId>& current_;
  std::pmr::vector<database::TrackId>& played_;
  std::pmr::vector<TrackQueue::Item>& enqueued_;
};

auto TrackQueue::Load(std::weak_ptr<database::Database> db) -> void {
  auto db_lock = db.lock();
  if (!db_lock) {
    return;
  }
  auto raw = db_lock->Get(kSerialiseKey);
  if (!raw) {
    return;
  }

  Parser p{db, current_, played_, enqueued_};
  const uint8_t* data = reinterpret_cast<const uint8_t*>(raw->data());
  cppbor::parse(data, data + raw->size(), &p);
}

}  // namespace audio
