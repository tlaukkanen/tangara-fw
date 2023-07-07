/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "track_queue.hpp"

#include <algorithm>
#include <mutex>

#include "audio_events.hpp"
#include "audio_fsm.hpp"
#include "event_queue.hpp"
#include "track.hpp"
#include "ui_fsm.hpp"

namespace audio {

TrackQueue::TrackQueue() {}

auto TrackQueue::GetCurrent() const -> std::optional<database::TrackId> {
  const std::lock_guard<std::mutex> lock(mutex_);
  return current_;
}

auto TrackQueue::GetUpcoming(std::size_t limit) const
    -> std::vector<database::TrackId> {
  const std::lock_guard<std::mutex> lock(mutex_);
  std::vector<database::TrackId> ret;
  limit = std::min(limit, upcoming_.size());
  std::for_each_n(upcoming_.begin(), limit,
                  [&](const auto i) { ret.push_back(i); });
  return ret;
}

auto TrackQueue::AddNext(database::TrackId t) -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (!current_) {
    current_ = t;
  } else {
    upcoming_.push_front(t);
  }

  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

auto TrackQueue::AddNext(const std::vector<database::TrackId>& t) -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  std::for_each(t.rbegin(), t.rend(),
                [&](const auto i) { upcoming_.push_front(i); });
  if (!current_) {
    current_ = upcoming_.front();
    upcoming_.pop_front();
  }
  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

auto TrackQueue::AddLast(database::TrackId t) -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (!current_) {
    current_ = t;
  } else {
    upcoming_.push_back(t);
  }

  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

auto TrackQueue::AddLast(const std::vector<database::TrackId>& t) -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  std::for_each(t.begin(), t.end(),
                [&](const auto i) { upcoming_.push_back(i); });
  if (!current_) {
    current_ = upcoming_.front();
    upcoming_.pop_front();
  }
  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

auto TrackQueue::Next() -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (current_) {
    played_.push_front(*current_);
  }
  if (!upcoming_.empty()) {
    current_ = upcoming_.front();
    upcoming_.pop_front();
  } else {
    current_.reset();
  }
  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

auto TrackQueue::Previous() -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (current_) {
    upcoming_.push_front(*current_);
  }
  if (!played_.empty()) {
    current_ = played_.front();
    played_.pop_front();
  } else {
    current_.reset();
  }
  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

auto TrackQueue::Clear() -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  played_.clear();
  upcoming_.clear();
  current_.reset();
  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

auto TrackQueue::RemoveUpcoming(database::TrackId t) -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  for (auto it = upcoming_.begin(); it != upcoming_.end(); it++) {
    if (*it == t) {
      upcoming_.erase(it);
      return;
    }
  }
  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

}  // namespace audio
