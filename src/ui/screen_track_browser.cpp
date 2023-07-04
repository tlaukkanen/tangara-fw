/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <algorithm>
#include <memory>

#include "database.hpp"
#include "event_queue.hpp"
#include "lvgl.h"
#include "screen_menu.hpp"

#include "core/lv_event.h"
#include "esp_log.h"

#include "core/lv_group.h"
#include "core/lv_obj_pos.h"
#include "extra/widgets/list/lv_list.h"
#include "extra/widgets/menu/lv_menu.h"
#include "extra/widgets/spinner/lv_spinner.h"
#include "hal/lv_hal_disp.h"
#include "misc/lv_area.h"
#include "screen_track_browser.hpp"
#include "ui_events.hpp"
#include "ui_fsm.hpp"
#include "widgets/lv_label.h"

static constexpr char kTag[] = "browser";

static constexpr int kMaxPages = 3;
static constexpr int kPageBuffer = 5;

namespace ui {
namespace screens {

static void item_click_cb(lv_event_t* ev) {
  if (ev->user_data == NULL) {
    return;
  }
  TrackBrowser* instance = reinterpret_cast<TrackBrowser*>(ev->user_data);
  instance->OnItemClicked(ev);
}

static void item_select_cb(lv_event_t* ev) {
  if (ev->user_data == NULL) {
    return;
  }
  TrackBrowser* instance = reinterpret_cast<TrackBrowser*>(ev->user_data);
  instance->OnItemSelected(ev);
}

TrackBrowser::TrackBrowser(
    std::weak_ptr<database::Database> db,
    const std::string& title,
    std::future<database::Result<database::IndexRecord>*>&& initial_page)
    : db_(db),
      list_(nullptr),
      loading_indicator_(nullptr),
      loading_pos_(END),
      loading_page_(std::move(initial_page)),
      current_pages_() {
  lv_obj_t* title_obj = lv_label_create(root_);
  lv_label_set_text(title_obj, title.c_str());

  list_ = lv_list_create(root_);
  lv_obj_set_size(list_, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
  lv_obj_center(list_);
}

auto TrackBrowser::Tick() -> void {
  if (!loading_page_) {
    return;
  }
  if (!loading_page_->valid()) {
    // TODO(jacqueline): error case.
    return;
  }
  if (loading_page_->wait_for(std::chrono::seconds(0)) ==
      std::future_status::ready) {
    auto result = loading_page_->get();
    AddResults(loading_pos_.value_or(END), result);

    loading_page_.reset();
    loading_pos_.reset();
  }
}

auto TrackBrowser::OnItemSelected(lv_event_t* ev) -> void {
  auto index = GetItemIndex(lv_event_get_target(ev));
  if (!index) {
    return;
  }
  if (index < kPageBuffer) {
    FetchNewPage(START);
    return;
  }
  if (index > GetNumRecords() - kPageBuffer) {
    FetchNewPage(END);
    return;
  }
}

auto TrackBrowser::OnItemClicked(lv_event_t* ev) -> void {
  auto index = GetItemIndex(lv_event_get_target(ev));
  if (!index) {
    return;
  }
  auto record = GetRecordByIndex(*index);
  if (!record) {
    return;
  }
  ESP_LOGI(kTag, "clicked item %u (%s)", *index,
           record->text().value_or("[nil]").c_str());
  events::Dispatch<internal::RecordSelected, UiState>(
      internal::RecordSelected{.record = *record});
}

auto TrackBrowser::AddLoadingIndictor(Position pos) -> void {
  if (loading_indicator_) {
    return;
  }
  loading_indicator_ = lv_list_add_text(list_, "Loading...");
  if (pos == START) {
    lv_obj_move_to_index(loading_indicator_, 0);
  }
}

auto TrackBrowser::AddResults(Position pos,
                              database::Result<database::IndexRecord>* results)
    -> void {
  if (loading_indicator_ != nullptr) {
    lv_obj_del(loading_indicator_);
    loading_indicator_ = nullptr;
  }

  auto fn = [&](const database::IndexRecord& record) {
    auto text = record.text();
    if (!text) {
      // TODO(jacqueline): Display category-specific text.
      text = "[ no data ]";
    }
    lv_obj_t* item = lv_list_add_btn(list_, NULL, text->c_str());
    lv_obj_add_event_cb(item, item_click_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(item, item_select_cb, LV_EVENT_FOCUSED, this);
    lv_group_add_obj(group_, item);
    if (pos == START) {
      lv_obj_move_to_index(item, 0);
    }
  };

  switch (pos) {
    case START:
      std::for_each(results->values().rbegin(), results->values().rend(), fn);
      current_pages_.emplace_front(results);
      break;
    case END:
      std::for_each(results->values().begin(), results->values().end(), fn);
      current_pages_.emplace_back(results);
      break;
  }
}

auto TrackBrowser::DropPage(Position pos) -> void {
  if (pos == START) {
    for (int i = 0; i < current_pages_.front()->values().size(); i++) {
      lv_obj_t* item = lv_obj_get_child(list_, 0);
      if (item == NULL) {
        continue;
      }
      lv_obj_del(item);
    }
    current_pages_.pop_front();
  } else if (pos == END) {
    for (int i = 0; i < current_pages_.back()->values().size(); i++) {
      lv_obj_t* item = lv_obj_get_child(list_, lv_obj_get_child_cnt(list_) - 1);
      if (item == NULL) {
        continue;
      }
      lv_group_remove_obj(item);
      lv_obj_del(item);
    }
    current_pages_.pop_back();
  }
}

auto TrackBrowser::FetchNewPage(Position pos) -> void {
  if (loading_page_) {
    return;
  }
  auto db = db_.lock();
  if (!db) {
    return;
  }

  // If we already have a complete set of pages, drop the page that's furthest
  // away.
  if (current_pages_.size() >= kMaxPages) {
    switch (pos) {
      case START:
        DropPage(END);
        break;
      case END:
        DropPage(START);
        break;
    }
  }

  std::optional<database::Continuation<database::IndexRecord>> cont;
  switch (pos) {
    case START:
      cont = current_pages_.front()->prev_page();
      break;
    case END:
      cont = current_pages_.back()->next_page();
      break;
  }
  if (!cont) {
    return;
  }

  loading_pos_ = pos;
  loading_page_ = db->GetPage(&cont.value());
}

auto TrackBrowser::GetNumRecords() -> std::size_t {
  return lv_obj_get_child_cnt(list_) - (loading_indicator_ != nullptr ? 1 : 0);
}

auto TrackBrowser::GetItemIndex(lv_obj_t* obj) -> std::optional<std::size_t> {
  std::size_t child_count = lv_obj_get_child_cnt(list_);
  std::size_t index = 0;
  for (int i = 0; i < child_count; i++) {
    lv_obj_t* child = lv_obj_get_child(list_, i);
    if (child == loading_indicator_) {
      continue;
    }
    if (child == obj) {
      return index;
    }
    index++;
  }
  return {};
}

auto TrackBrowser::GetRecordByIndex(std::size_t index)
    -> std::optional<database::IndexRecord> {
  std::size_t current_index = 0;
  for (const auto& page : current_pages_) {
    if (index > current_index + page->values().size()) {
      current_index += page->values().size();
      continue;
    }
    if (index < current_index) {
      // uhhh
      break;
    }
    std::size_t index_in_page = index - current_index;
    return page->values().at(index_in_page);
  }
  return {};
}

}  // namespace screens
}  // namespace ui
