/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "lvgl.h"

#include "database.hpp"
#include "model_top_bar.hpp"
#include "screen.hpp"
#include "track_queue.hpp"

namespace ui {
namespace screens {

class TrackBrowser : public Screen {
 public:
  TrackBrowser(
      models::TopBar& top_bar,
      audio::TrackQueue& queue,
      std::weak_ptr<database::Database> db,
      const std::pmr::vector<std::pmr::string>& breadcrumbs,
      std::future<database::Result<database::IndexRecord>*>&& initial_page);
  ~TrackBrowser() {}

  auto Tick() -> void override;

  auto OnItemSelected(lv_event_t* ev) -> void;
  auto OnItemClicked(lv_event_t* ev) -> void;

 private:
  enum Position {
    START = 0,
    END = 1,
  };
  auto AddLoadingIndictor(Position pos) -> void;
  auto AddResults(Position pos,
                  std::shared_ptr<database::Result<database::IndexRecord>>)
      -> void;
  auto DropPage(Position pos) -> void;
  auto FetchNewPage(Position pos) -> void;

  auto GetNumRecords() -> std::size_t;
  auto GetItemIndex(lv_obj_t* obj) -> std::optional<std::size_t>;

  audio::TrackQueue& queue_;
  std::weak_ptr<database::Database> db_;
  lv_obj_t* back_button_;
  lv_obj_t* play_button_;
  lv_obj_t* enqueue_button_;
  lv_obj_t* list_;
  lv_obj_t* loading_indicator_;

  std::pmr::vector<std::pmr::string> breadcrumbs_;

  std::optional<Position> loading_pos_;
  std::optional<std::future<database::Result<database::IndexRecord>*>>
      loading_page_;

  std::shared_ptr<database::Result<database::IndexRecord>> initial_page_;
  std::deque<std::shared_ptr<database::Result<database::IndexRecord>>>
      current_pages_;
};

}  // namespace screens
}  // namespace ui
