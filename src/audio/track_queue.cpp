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

[[maybe_unused]] static constexpr char kTag[] = "tracks";

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

  QueueUpdate ev{.current_changed = enqueued_.size() < 2};
  events::Audio().Dispatch(ev);
  events::Ui().Dispatch(ev);
}

auto TrackQueue::AddNext(std::shared_ptr<playlist::ISource> src) -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  enqueued_.push_front(src);

  QueueUpdate ev{.current_changed = enqueued_.size() < 2};
  events::Audio().Dispatch(ev);
  events::Ui().Dispatch(ev);
}

auto TrackQueue::IncludeNext(std::shared_ptr<playlist::IResetableSource> src)
    -> void {
  assert(src.get() != nullptr);
  const std::lock_guard<std::mutex> lock(mutex_);
  enqueued_.push_front(src);

  QueueUpdate ev{.current_changed = enqueued_.size() < 2};
  events::Audio().Dispatch(ev);
  events::Ui().Dispatch(ev);
}

auto TrackQueue::AddLast(database::TrackId t) -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  enqueued_.push_back(t);

  QueueUpdate ev{.current_changed = enqueued_.size() < 2};
  events::Audio().Dispatch(ev);
  events::Ui().Dispatch(ev);
}

auto TrackQueue::AddLast(std::shared_ptr<playlist::ISource> src) -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  enqueued_.push_back(src);

  QueueUpdate ev{.current_changed = enqueued_.size() < 2};
  events::Audio().Dispatch(ev);
  events::Ui().Dispatch(ev);
}

auto TrackQueue::IncludeLast(std::shared_ptr<playlist::IResetableSource> src)
    -> void {
  assert(src.get() != nullptr);
  const std::lock_guard<std::mutex> lock(mutex_);
  enqueued_.push_back(src);

  QueueUpdate ev{.current_changed = enqueued_.size() < 2};
  events::Audio().Dispatch(ev);
  events::Ui().Dispatch(ev);
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

  QueueUpdate ev{.current_changed = true};
  events::Audio().Dispatch(ev);
  events::Ui().Dispatch(ev);
}

auto TrackQueue::Previous() -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (!enqueued_.empty() &&
      std::holds_alternative<std::shared_ptr<playlist::IResetableSource>>(
          enqueued_.front())) {
    auto src = std::get<std::shared_ptr<playlist::IResetableSource>>(
        enqueued_.front());
    if (src->Previous()) {
      QueueUpdate ev{.current_changed = false};
      events::Audio().Dispatch(ev);
      events::Ui().Dispatch(ev);
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

  QueueUpdate ev{.current_changed = true};
  events::Audio().Dispatch(ev);
  events::Ui().Dispatch(ev);
}

auto TrackQueue::Clear() -> void {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (enqueued_.empty() && played_.empty()) {
    return;
  }
  QueueUpdate ev{.current_changed = !enqueued_.empty()};
  played_.clear();
  enqueued_.clear();

  events::Audio().Dispatch(ev);
  events::Ui().Dispatch(ev);
}

}  // namespace audio
