/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "shuffler.hpp"

#include <algorithm>
#include <functional>
#include <memory>
#include <set>
#include <variant>

#include "bloom_filter.hpp"
#include "database.hpp"
#include "komihash.h"
#include "random.hpp"
#include "track.hpp"

static constexpr std::size_t kShufflerBufferSize = 32;

namespace playlist {

auto Shuffler::Create() -> Shuffler* {
  return new Shuffler(util::sRandom,
                      std::make_unique<util::BloomFilter<database::TrackId>>(
                          [](database::TrackId id) {
                            return komihash(&id, sizeof(database::TrackId), 0);
                          }));
}

Shuffler::Shuffler(util::IRandom* random,
                   std::unique_ptr<util::BloomFilter<database::TrackId>> filter)
    : random_(random), already_played_(std::move(filter)) {}

auto Shuffler::Current() -> std::optional<database::TrackId> {
  if (shuffled_items_buffer_.empty()) {
    return {};
  }
  return shuffled_items_buffer_.front();
}

auto Shuffler::Advance() -> std::optional<database::TrackId> {
  if (shuffled_items_buffer_.empty() && !out_of_items_) {
    RefillBuffer();
  }

  auto res = Current();
  if (res) {
    // Mark tracks off in the bloom filter only *after* they've been advanced
    // past. This gives us the most flexibility for reshuffling when adding new
    // items.
    already_played_->Insert(*res);
    shuffled_items_buffer_.pop_front();
  }
  return res;
}

auto Shuffler::Peek(std::size_t num, std::vector<database::TrackId>* out)
    -> std::size_t {
  if (shuffled_items_buffer_.size() < num) {
    RefillBuffer();
  }
  for (int i = 0; i < num; i++) {
    if (i >= shuffled_items_buffer_.size()) {
      // We must be out of data, since the buffer didn't fill up.
      return i;
    }
    out->push_back(shuffled_items_buffer_.at(i));
  }
  return num;
}

auto Shuffler::Add(Item item) -> void {
  ordered_items_.push_back(item);
  out_of_items_ = false;

  // Empty out the buffer of already shuffled items, since we will need to
  // shuffle again in order to incorporate the newly added item(s). We keep the
  // current item however because we wouldn't want Add() to change the value of
  // Current() unless we're completely out of items.
  if (shuffled_items_buffer_.size() > 1) {
    shuffled_items_buffer_.erase(shuffled_items_buffer_.begin() + 1,
                                 shuffled_items_buffer_.end());
  }
  RefillBuffer();
}

auto Shuffler::Unshuffle() -> std::vector<Item> {
  std::vector<Item> ret;
  database::TrackId current = shuffled_items_buffer_.front();
  bool has_found_current = false;

  for (const Item& item : ordered_items_) {
    if (!has_found_current) {
      // TODO(jacqueline): *Should* this include previous items? What is the
      // 'previous' button meant to do after unshuffling?
      if (std::holds_alternative<database::TrackId>(item)) {
        has_found_current = current == std::get<database::TrackId>(item);
      } else {
        auto source = std::get<std::shared_ptr<IResetableSource>>(item);
        source->Reset();
        has_found_current =
            std::get<std::shared_ptr<IResetableSource>>(item)->AdvanceTo(
                current);
      }
    } else {
      ret.push_back(item);
    }
  }

  return ret;
}

auto Shuffler::RefillBuffer() -> void {
  // Don't waste time iterating if we know there's nothing new.
  if (out_of_items_) {
    return;
  }

  int num_to_sample = kShufflerBufferSize - shuffled_items_buffer_.size();
  int resovoir_offset = shuffled_items_buffer_.size();

  std::set<database::TrackId> in_buffer;
  for (const database::TrackId& id : shuffled_items_buffer_) {
    in_buffer.insert(id);
  }

  uint32_t i = 0;
  auto consider_item = [&, this](const database::TrackId& item) {
    if (already_played_->Contains(item) || in_buffer.contains(item)) {
      return;
    }
    if (i < num_to_sample) {
      shuffled_items_buffer_.push_back(item);
    } else {
      uint32_t index_to_replace = random_->RangeInclusive(0, i);
      if (index_to_replace < num_to_sample) {
        shuffled_items_buffer_[resovoir_offset + index_to_replace] = item;
      }
    }
    i++;
  };

  for (const Item& item : ordered_items_) {
    if (std::holds_alternative<database::TrackId>(item)) {
      std::invoke(consider_item, std::get<database::TrackId>(item));
    } else {
      auto source = std::get<std::shared_ptr<IResetableSource>>(item);
      source->Reset();
      while (source->Advance()) {
        std::invoke(consider_item, *source->Current());
      }
    }
  }

  out_of_items_ = i > num_to_sample;
  // We've now got a random *selection*, but the order might be predictable
  // (e.g. if there were only `num_to_sample` new items). Do a final in-memory
  // shuffle.
  std::random_shuffle(shuffled_items_buffer_.begin() + resovoir_offset,
                      shuffled_items_buffer_.end());
}

}  // namespace playlist
