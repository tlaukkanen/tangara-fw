/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "track_queue.hpp"

#include <algorithm>
#include <mutex>
#include <variant>

#include "audio_events.hpp"
#include "audio_fsm.hpp"
#include "event_queue.hpp"
#include "source.hpp"
#include "track.hpp"
#include "ui_fsm.hpp"

namespace audio {

TrackQueue::TrackQueue() {}

auto TrackQueue::GetCurrent() const -> std::optional<database::TrackId> {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (enqueued_.empty()) {
    return {};
  }
  auto item = enqueued_.front();
  if (std::holds_alternative<database::TrackId>(item)) {
    return std::get<database::TrackId>(item);
  }
  if (std::holds_alternative<std::shared_ptr<playlist::ISource>>(item)) {
    return std::get<std::shared_ptr<playlist::ISource>>(item)->Current();
  }
  if (std::holds_alternative<std::shared_ptr<playlist::IResetableSource>>(
          item)) {
    return std::get<std::shared_ptr<playlist::IResetableSource>>(item)
        ->Current();
  }
  return {};
}

auto TrackQueue::GetUpcoming(std::size_t limit) const
    -> std::vector<database::TrackId> {
  const std::lock_guard<std::mutex> lock(mutex_);
  std::vector<database::TrackId> ret;

  auto it = enqueued_.begin();
  if (it == enqueued_.end()) {
    return ret;
  }

  // Don't include the current track. This is only relevant to raw track ids,
  // since sources include multiple tracks.
  if (std::holds_alternative<database::TrackId>(*it)) {
    it++;
  }

  while (limit > 0 && it != enqueued_.end()) {
    auto item = *it;
    if (std::holds_alternative<database::TrackId>(item)) {
      ret.push_back(std::get<database::TrackId>(item));
      limit--;
    } else if (std::holds_alternative<std::shared_ptr<playlist::ISource>>(
                   item)) {
      limit -=
          std::get<std::shared_ptr<playlist::ISource>>(item)->Peek(limit, &ret);
    } else if (std::holds_alternative<
                   std::shared_ptr<playlist::IResetableSource>>(item)) {
      limit -=
          std::get<std::shared_ptr<playlist::IResetableSource>>(item)->Peek(
              limit, &ret);
    }
    it++;
  }

  return ret;
}

auto TrackQueue::AddNext(database::TrackId t) -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  enqueued_.push_front(t);
  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

auto TrackQueue::AddNext(std::shared_ptr<playlist::ISource> src) -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  enqueued_.push_front(src);
  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

auto TrackQueue::IncludeNext(std::shared_ptr<playlist::IResetableSource> src)
    -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  enqueued_.push_front(src);
  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

auto TrackQueue::AddLast(database::TrackId t) -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  enqueued_.push_back(t);
  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

auto TrackQueue::AddLast(std::shared_ptr<playlist::ISource> src) -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  enqueued_.push_back(src);
  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

auto TrackQueue::IncludeLast(std::shared_ptr<playlist::IResetableSource> src)
    -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  enqueued_.push_back(src);
  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

auto TrackQueue::Next() -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (enqueued_.empty()) {
    return;
  }

  auto item = enqueued_.front();
  if (std::holds_alternative<database::TrackId>(item)) {
    played_.push_front(std::get<database::TrackId>(item));
    enqueued_.pop_front();
  }
  if (std::holds_alternative<std::shared_ptr<playlist::ISource>>(item)) {
    auto src = std::get<std::shared_ptr<playlist::ISource>>(item);
    played_.push_front(*src->Current());
    if (!src->Advance()) {
      enqueued_.pop_front();
    }
  }
  if (std::holds_alternative<std::shared_ptr<playlist::IResetableSource>>(
          item)) {
    auto src = std::get<std::shared_ptr<playlist::IResetableSource>>(item);
    if (!src->Advance()) {
      played_.push_back(src);
      enqueued_.pop_front();
    }
  }

  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

auto TrackQueue::Previous() -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (!enqueued_.empty() &&
      std::holds_alternative<std::shared_ptr<playlist::IResetableSource>>(
          enqueued_.front())) {
    auto src = std::get<std::shared_ptr<playlist::IResetableSource>>(
        enqueued_.front());
    if (src->Previous()) {
      events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
      return;
    }
  }

  if (played_.empty()) {
    return;
  }

  auto item = played_.front();
  if (std::holds_alternative<database::TrackId>(item)) {
    enqueued_.push_front(std::get<database::TrackId>(item));
  } else if (std::holds_alternative<
                 std::shared_ptr<playlist::IResetableSource>>(item)) {
    enqueued_.push_front(
        std::get<std::shared_ptr<playlist::IResetableSource>>(item));
  }
  played_.pop_front();

  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

auto TrackQueue::Clear() -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  played_.clear();
  enqueued_.clear();
  events::Dispatch<QueueUpdate, AudioState, ui::UiState>({});
}

}  // namespace audio
