/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "source.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <variant>

#include "esp_log.h"

#include "bloom_filter.hpp"
#include "database.hpp"
#include "komihash.h"
#include "random.hpp"
#include "track.hpp"

namespace playlist {

IndexRecordSource::IndexRecordSource(
    std::weak_ptr<database::Database> db,
    std::shared_ptr<database::Result<database::IndexRecord>> initial)
    : db_(db),
      initial_page_(initial),
      initial_item_(0),
      current_page_(initial_page_),
      current_item_(initial_item_) {}

IndexRecordSource::IndexRecordSource(
    std::weak_ptr<database::Database> db,
    std::shared_ptr<database::Result<database::IndexRecord>> initial,
    std::size_t initial_index,
    std::shared_ptr<database::Result<database::IndexRecord>> current,
    std::size_t current_index)
    : db_(db),
      initial_page_(initial),
      initial_item_(initial_index),
      current_page_(current),
      current_item_(current_index) {}

auto IndexRecordSource::Current() -> std::optional<database::TrackId> {
  if (current_page_->values().size() <= current_item_) {
    return {};
  }
  if (current_page_ == initial_page_ && current_item_ < initial_item_) {
    return {};
  }

  return current_page_->values().at(current_item_).track();
}

auto IndexRecordSource::Advance() -> std::optional<database::TrackId> {
  current_item_++;
  if (current_item_ >= current_page_->values().size()) {
    auto next_page = current_page_->next_page();
    if (!next_page) {
      current_item_--;
      return {};
    }

    auto db = db_.lock();
    if (!db) {
      return {};
    }

    current_page_.reset(db->GetPage(&*next_page).get());
    current_item_ = 0;
  }

  return Current();
}

auto IndexRecordSource::Previous() -> std::optional<database::TrackId> {
  if (current_page_ == initial_page_ && current_item_ <= initial_item_) {
    return {};
  }

  current_item_--;
  if (current_item_ < 0) {
    auto prev_page = current_page_->prev_page();
    if (!prev_page) {
      return {};
    }

    auto db = db_.lock();
    if (!db) {
      return {};
    }

    current_page_.reset(db->GetPage(&*prev_page).get());
    current_item_ = current_page_->values().size() - 1;
  }

  return Current();
}

auto IndexRecordSource::Peek(std::size_t n, std::vector<database::TrackId>* out)
    -> std::size_t {
  if (current_page_->values().size() <= current_item_) {
    return {};
  }

  auto db = db_.lock();
  if (!db) {
    return 0;
  }

  std::size_t items_added = 0;

  std::shared_ptr<database::Result<database::IndexRecord>> working_page =
      current_page_;
  std::size_t working_item = current_item_ + 1;

  while (n > 0) {
    if (working_item >= working_page->values().size()) {
      auto next_page = current_page_->next_page();
      if (!next_page) {
        break;
      }
      // TODO(jacqueline): It would probably be a good idea to hold onto these
      // peeked pages, to avoid needing to look them up again later.
      working_page.reset(db->GetPage(&*next_page).get());
      working_item = 0;
    }

    out->push_back(working_page->values().at(working_item).track().value());
    n--;
    items_added++;
    working_item++;
  }

  return items_added;
}

auto IndexRecordSource::Reset() -> void {
  current_page_ = initial_page_;
  current_item_ = initial_item_;
}

}  // namespace playlist
