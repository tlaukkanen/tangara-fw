/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <algorithm>
#include <memory>

#include "core/lv_obj.h"
#include "core/lv_obj_scroll.h"
#include "core/lv_obj_tree.h"
#include "database.hpp"
#include "event_queue.hpp"
#include "extra/layouts/flex/lv_flex.h"
#include "font/lv_symbol_def.h"
#include "lvgl.h"
#include "misc/lv_anim.h"
#include "misc/lv_color.h"
#include "model_top_bar.hpp"
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
#include "source.hpp"
#include "themes.hpp"
#include "track_queue.hpp"
#include "ui_events.hpp"
#include "ui_fsm.hpp"
#include "widget_top_bar.hpp"
#include "widgets/lv_label.h"

[[maybe_unused]] static constexpr char kTag[] = "browser";

static constexpr int kMaxPages = 4;
static constexpr int kPageBuffer = 6;

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
    models::TopBar& top_bar_model,
    audio::TrackQueue& queue,
    std::weak_ptr<database::Database> db,
    const std::pmr::vector<std::pmr::string>& crumbs,
    std::future<database::Result<database::IndexRecord>*>&& initial_page)
    : queue_(queue),
      db_(db),
      play_button_(nullptr),
      enqueue_button_(nullptr),
      list_(nullptr),
      loading_indicator_(nullptr),
      breadcrumbs_(crumbs),
      loading_pos_(END),
      loading_page_(move(initial_page)),
      initial_page_(),
      current_pages_() {
  lv_obj_set_layout(content_, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);

  widgets::TopBar::Configuration config{
      .show_back_button = true,
      .title = breadcrumbs_[0],
  };
  auto top_bar = CreateTopBar(content_, config, top_bar_model);
  back_button_ = top_bar->button();

  lv_obj_t* scrollable = lv_obj_create(content_);
  lv_obj_set_width(scrollable, lv_pct(100));
  lv_obj_set_flex_grow(scrollable, 1);
  lv_obj_set_layout(scrollable, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(scrollable, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(scrollable, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);

  if (crumbs.size() > 1) {
    lv_obj_t* header = lv_obj_create(scrollable);
    lv_obj_set_size(header, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(header, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    lv_obj_set_style_pad_left(header, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_right(header, 4, LV_PART_MAIN);

    lv_obj_t* spacer = lv_obj_create(header);
    lv_obj_set_size(spacer, 1, 2);

    for (size_t i = 1; i < crumbs.size(); i++) {
      lv_obj_t* crumb = lv_label_create(header);
      lv_label_set_text(crumb, crumbs[i].c_str());

      spacer = lv_obj_create(header);
      lv_obj_set_size(spacer, 1, 2);
    }

    spacer = lv_obj_create(header);
    lv_obj_set_size(spacer, 1, 2);

    lv_obj_t* buttons_container = lv_obj_create(header);
    lv_obj_set_width(buttons_container, lv_pct(100));
    lv_obj_set_height(buttons_container, LV_SIZE_CONTENT);
    lv_obj_set_layout(buttons_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(buttons_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(buttons_container, LV_FLEX_ALIGN_END,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* label;

    play_button_ = lv_btn_create(buttons_container);
    label = lv_label_create(play_button_);
    lv_label_set_text(label, "Play all");
    lv_group_add_obj(group_, play_button_);
    themes::Theme::instance()->ApplyStyle(play_button_,
                                          themes::Style::kButtonPrimary);

    lv_bind(play_button_, LV_EVENT_CLICKED, [&](lv_obj_t*) {
      if (!initial_page_) {
        return;
      }
      queue_.Clear();
      queue_.IncludeNext(playlist::CreateSourceFromResults(db_, initial_page_));
      events::Ui().Dispatch(internal::ShowNowPlaying{});
    });

    if (queue_.GetCurrent()) {
      spacer = lv_obj_create(buttons_container);
      lv_obj_set_size(spacer, 4, 1);

      enqueue_button_ = lv_btn_create(buttons_container);
      label = lv_label_create(enqueue_button_);
      lv_label_set_text(label, "Enqueue");
      lv_group_add_obj(group_, enqueue_button_);
      themes::Theme::instance()->ApplyStyle(enqueue_button_,
                                            themes::Style::kButtonPrimary);

      lv_bind(enqueue_button_, LV_EVENT_CLICKED, [&](lv_obj_t*) {
        if (!initial_page_) {
          return;
        }
        queue_.IncludeNext(
            playlist::CreateSourceFromResults(db_, initial_page_));
      });
    }

    lv_obj_set_style_border_width(header, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(header, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, LV_PART_MAIN);

    spacer = lv_obj_create(header);
    lv_obj_set_size(spacer, 1, 4);

    lv_obj_set_style_border_width(header, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(
        header, lv_palette_lighten(LV_PALETTE_GREY, 3), LV_PART_MAIN);
  }

  list_ = lv_list_create(scrollable);
  lv_obj_set_size(list_, lv_pct(100), LV_SIZE_CONTENT);

  // The default scrollbar is deceptive because we load in items progressively.
  lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
  // Wrapping behaves in surprising ways, again due to progressing loading.
  lv_group_set_wrap(group_, false);
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
    std::shared_ptr<database::Result<database::IndexRecord>> result{
        loading_page_->get()};
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
  auto res = GetItemIndex(lv_event_get_target(ev));
  if (!res) {
    return;
  }

  auto index = *res;
  for (const auto& page : current_pages_) {
    for (std::size_t i = 0; i < page->values().size(); i++) {
      if (index == 0) {
        auto text = page->values()[i]->text();
        auto crumbs = breadcrumbs_;
        crumbs.push_back(text.value());
        events::Ui().Dispatch(internal::RecordSelected{
            .show_menu = ev->code == LV_EVENT_LONG_PRESSED,
            .new_crumbs = crumbs,
            .initial_page = initial_page_,
            .page = page,
            .record = i,
        });
        return;
      }
      index--;
    }
  }
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

auto TrackBrowser::AddResults(
    Position pos,
    std::shared_ptr<database::Result<database::IndexRecord>> results) -> void {
  if (loading_indicator_ != nullptr) {
    lv_obj_del(loading_indicator_);
    loading_indicator_ = nullptr;
  }

  if (initial_page_ == nullptr) {
    initial_page_ = results;
  }

  auto fn = [&](const std::shared_ptr<database::IndexRecord>& record) {
    auto text = record->text();
    if (!text) {
      // TODO(jacqueline): Display category-specific text.
      text = "[ no data ]";
    }
    lv_obj_t* item = lv_list_add_btn(list_, NULL, text->c_str());
    lv_label_set_long_mode(lv_obj_get_child(item, -1), LV_LABEL_LONG_DOT);
    lv_obj_add_event_cb(item, item_click_cb, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(item, item_click_cb, LV_EVENT_LONG_PRESSED, this);
    lv_obj_add_event_cb(item, item_select_cb, LV_EVENT_FOCUSED, this);

    if (pos == START) {
      lv_obj_move_to_index(item, 0);
    }
  };

  lv_obj_t* focused = lv_group_get_focused(group_);

  // Adding objects at the start of the list will artificially scroll the list
  // up. Scroll it down by the height we're adding so that the user doesn't
  // notice any jank.
  if (pos == START) {
    int num_to_add = results->values().size();
    // Assuming that all items are the same height, this item's y pos should be
    // exactly the height of the new items.
    lv_obj_t* representative_item = lv_obj_get_child(list_, num_to_add);
    if (representative_item != nullptr) {
      int scroll_adjustment = lv_obj_get_y(representative_item);
      lv_obj_scroll_by(list_, 0, -scroll_adjustment, LV_ANIM_OFF);
    }
  }

  switch (pos) {
    case START:
      std::for_each(results->values().rbegin(), results->values().rend(), fn);
      current_pages_.push_front(results);
      break;
    case END:
      std::for_each(results->values().begin(), results->values().end(), fn);
      current_pages_.push_back(results);
      break;
  }

  lv_group_remove_all_objs(group_);
  lv_group_add_obj(group_, back_button_);
  if (play_button_) {
    lv_group_add_obj(group_, play_button_);
  }
  if (enqueue_button_) {
    lv_group_add_obj(group_, enqueue_button_);
  }
  int num_children = lv_obj_get_child_cnt(list_);
  for (int i = 0; i < num_children; i++) {
    lv_group_add_obj(group_, lv_obj_get_child(list_, i));
  }
  lv_group_focus_obj(focused);
}

auto TrackBrowser::DropPage(Position pos) -> void {
  if (pos == START) {
    // Removing objects from the start of the list will artificially scroll the
    // list down. Scroll it up by the height we're removing so that the user
    // doesn't notice any jank.
    int num_to_remove = current_pages_.front()->values().size();
    lv_obj_t* new_top_obj = lv_obj_get_child(list_, num_to_remove);
    if (new_top_obj != nullptr) {
      int scroll_adjustment = lv_obj_get_y(new_top_obj);
      lv_obj_scroll_by(list_, 0, scroll_adjustment, LV_ANIM_OFF);
    }

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

  std::optional<database::Continuation> cont;
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

  loading_pos_ = pos;
  loading_page_ = db->GetPage<database::IndexRecord>(&cont.value());
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

}  // namespace screens
}  // namespace ui
