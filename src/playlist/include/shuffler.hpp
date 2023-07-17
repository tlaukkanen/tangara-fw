/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <variant>
#include <vector>

#include "bloom_filter.hpp"
#include "database.hpp"
#include "future_fetcher.hpp"
#include "random.hpp"
#include "source.hpp"
#include "track.hpp"

namespace playlist {

/*
 * A source composes of other sources and/or specific extra tracks. Supports
 * iteration over its contents in a random order.
 */
class Shuffler : public ISource {
 public:
  static auto Create() -> Shuffler*;

  explicit Shuffler(
      util::IRandom* random,
      std::unique_ptr<util::BloomFilter<database::TrackId>> filter);

  auto Current() -> std::optional<database::TrackId> override;
  auto Advance() -> std::optional<database::TrackId> override;
  auto Peek(std::size_t, std::vector<database::TrackId>*)
      -> std::size_t override;

  typedef std::variant<database::TrackId, std::shared_ptr<IResetableSource>>
      Item;
  auto Add(Item) -> void;

  /*
   * Returns the enqueued items, starting from the current item, in their
   * original insertion order.
   */
  auto Unshuffle() -> std::vector<Item>;

  // Not copyable or movable.

  Shuffler(const Shuffler&) = delete;
  Shuffler& operator=(const Shuffler&) = delete;

 private:
  auto RefillBuffer() -> void;

  util::IRandom* random_;

  std::unique_ptr<util::BloomFilter<database::TrackId>> already_played_;
  bool out_of_items_;

  std::deque<Item> ordered_items_;
  std::deque<database::TrackId> shuffled_items_buffer_;
};

}  // namespace playlist
