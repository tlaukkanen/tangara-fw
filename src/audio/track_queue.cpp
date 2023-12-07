/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "track_queue.hpp"
#include <stdint.h>

#include <algorithm>
#include <memory>
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
#include "tasks.hpp"
#include "track.hpp"
#include "ui_fsm.hpp"

namespace audio {

[[maybe_unused]] static constexpr char kTag[] = "tracks";

auto notifyChanged(bool current_changed) -> void {
  QueueUpdate ev{.current_changed = current_changed};
  events::Ui().Dispatch(ev);
  events::Audio().Dispatch(ev);
}

TrackQueue::TrackQueue(tasks::Worker& bg_worker)
    : mutex_(),
      bg_worker_(bg_worker),
      pos_(0),
      tracks_(&memory::kSpiRamResource) {}

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

auto TrackQueue::insert(Item i) -> void {
  bool current_changed = pos_ == tracks_.size();
  if (std::holds_alternative<database::TrackId>(i)) {
    const std::unique_lock<std::shared_mutex> lock(mutex_);
    tracks_.push_back(std::get<database::TrackId>(i));
    notifyChanged(current_changed);
  } else if (std::holds_alternative<database::TrackIterator>(i)) {
    bg_worker_.Dispatch<void>([=, this]() {
      database::TrackIterator it = std::get<database::TrackIterator>(i);
      size_t working_pos = pos_;
      while (true) {
        auto next = *it;
        if (!next) {
          break;
        }
        const std::unique_lock<std::shared_mutex> lock(mutex_);
        tracks_.insert(tracks_.begin() + working_pos, *next);
        working_pos++;
        it++;
      }
      notifyChanged(current_changed);
    });
  }
}

auto TrackQueue::append(Item i) -> void {
  bool current_changed = pos_ == tracks_.size();
  if (std::holds_alternative<database::TrackId>(i)) {
    const std::unique_lock<std::shared_mutex> lock(mutex_);
    tracks_.push_back(std::get<database::TrackId>(i));
    notifyChanged(current_changed);
  } else if (std::holds_alternative<database::TrackIterator>(i)) {
    bg_worker_.Dispatch<void>([=, this]() {
      database::TrackIterator it = std::get<database::TrackIterator>(i);
      while (true) {
        auto next = *it;
        if (!next) {
          break;
        }
        const std::unique_lock<std::shared_mutex> lock(mutex_);
        tracks_.push_back(*next);
        it++;
      }
      notifyChanged(current_changed);
    });
  }
}

auto TrackQueue::next() -> void {
  const std::unique_lock<std::shared_mutex> lock(mutex_);
  pos_ = std::min<size_t>(pos_ + 1, tracks_.size());

  notifyChanged(true);
}

auto TrackQueue::previous() -> void {
  const std::unique_lock<std::shared_mutex> lock(mutex_);
  if (pos_ > 0) {
    pos_--;
  }

  notifyChanged(true);
}

auto TrackQueue::skipTo(database::TrackId id) -> void {
  const std::unique_lock<std::shared_mutex> lock(mutex_);
  for (size_t i = pos_; i < tracks_.size(); i++) {
    if (tracks_[i] == id) {
      pos_ = i;
    }
  }

  notifyChanged(true);
}

auto TrackQueue::clear() -> void {
  const std::unique_lock<std::shared_mutex> lock(mutex_);
  pos_ = 0;
  tracks_.clear();

  notifyChanged(true);
}

}  // namespace audio
