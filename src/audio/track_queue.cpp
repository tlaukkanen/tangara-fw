/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "track_queue.hpp"

#include <algorithm>
#include <mutex>
#include <optional>
#include <variant>

#include "audio_events.hpp"
#include "audio_fsm.hpp"
#include "database.hpp"
#include "event_queue.hpp"
#include "memory_resource.hpp"
#include "source.hpp"
#include "track.hpp"
#include "ui_fsm.hpp"

namespace audio {

[[maybe_unused]] static constexpr char kTag[] = "tracks";

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

}  // namespace audio
