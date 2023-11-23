/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <deque>
#include <memory>
#include <mutex>
#include <stack>
#include <variant>
#include <vector>

#include "bloom_filter.hpp"
#include "database.hpp"
#include "future_fetcher.hpp"
#include "random.hpp"
#include "track.hpp"

namespace playlist {

/*
 * Stateful interface for iterating over a collection of tracks by id.
 */
class ISource {
 public:
  virtual ~ISource() {}

  virtual auto Current() -> std::optional<database::TrackId> = 0;

  /*
   * Discards the current track id and continues to the next in this source.
   * Returns the new current track id.
   */
  virtual auto Advance() -> std::optional<database::TrackId> = 0;

  /*
   * Repeatedly advances until a track with the given id is the current track.
   * Returns false if this source ran out of tracks before the requested id
   * was encounted, true otherwise.
   */
  virtual auto AdvanceTo(database::TrackId id) -> bool {
    for (auto t = Current(); t.has_value(); t = Advance()) {
      if (*t == id) {
        return true;
      }
    }
    return false;
  }

  /*
   * Places the next n tracks into the given vector, in order. Does not change
   * the value returned by Current().
   */
  virtual auto Peek(std::size_t n, std::vector<database::TrackId>*)
      -> std::size_t = 0;
};

/*
 * A Source that supports restarting iteration from its original initial
 * value.
 */
class IResetableSource : public ISource {
 public:
  virtual ~IResetableSource() {}

  virtual auto Previous() -> std::optional<database::TrackId> = 0;

  /*
   * Restarts iteration from this source's initial value.
   */
  virtual auto Reset() -> void = 0;
};

class IteratorSource : public IResetableSource {
 public:
  IteratorSource(const database::Iterator&);

  auto Current() -> std::optional<database::TrackId> override;
  auto Advance() -> std::optional<database::TrackId> override;
  auto Peek(std::size_t n, std::vector<database::TrackId>*)
      -> std::size_t override;

  auto Previous() -> std::optional<database::TrackId> override;
  auto Reset() -> void override;

 private:
  const database::Iterator& start_;
  std::optional<database::TrackId> current_;
  std::stack<database::Iterator, std::vector<database::Iterator>> next_;
};

auto CreateSourceFromResults(
    std::weak_ptr<database::Database>,
    std::shared_ptr<database::Result<database::IndexRecord>>)
    -> std::shared_ptr<IResetableSource>;

class IndexRecordSource : public IResetableSource {
 public:
  IndexRecordSource(std::weak_ptr<database::Database> db,
                    std::shared_ptr<database::Result<database::IndexRecord>>);

  IndexRecordSource(std::weak_ptr<database::Database> db,
                    std::shared_ptr<database::Result<database::IndexRecord>>,
                    std::size_t,
                    std::shared_ptr<database::Result<database::IndexRecord>>,
                    std::size_t);

  auto Current() -> std::optional<database::TrackId> override;
  auto Advance() -> std::optional<database::TrackId> override;
  auto Peek(std::size_t n, std::vector<database::TrackId>*)
      -> std::size_t override;

  auto Previous() -> std::optional<database::TrackId> override;
  auto Reset() -> void override;

 private:
  std::weak_ptr<database::Database> db_;

  std::shared_ptr<database::Result<database::IndexRecord>> initial_page_;
  ssize_t initial_item_;

  std::shared_ptr<database::Result<database::IndexRecord>> current_page_;
  ssize_t current_item_;
};

class NestedSource : public IResetableSource {
 public:
  NestedSource(std::weak_ptr<database::Database> db,
               std::shared_ptr<database::Result<database::IndexRecord>>);

  auto Current() -> std::optional<database::TrackId> override;
  auto Advance() -> std::optional<database::TrackId> override;
  auto Peek(std::size_t n, std::vector<database::TrackId>*)
      -> std::size_t override;

  auto Previous() -> std::optional<database::TrackId> override;
  auto Reset() -> void override;

 private:
  auto CreateChild(std::shared_ptr<database::IndexRecord> page)
      -> std::shared_ptr<IResetableSource>;

  std::weak_ptr<database::Database> db_;

  std::shared_ptr<database::Result<database::IndexRecord>> initial_page_;
  ssize_t initial_item_;

  std::shared_ptr<database::Result<database::IndexRecord>> current_page_;
  ssize_t current_item_;

  std::shared_ptr<IResetableSource> current_child_;
};

}  // namespace playlist
