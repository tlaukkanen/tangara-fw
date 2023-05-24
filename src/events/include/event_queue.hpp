/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <functional>
#include <type_traits>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "tinyfsm.hpp"

#include "ui_fsm.hpp"

namespace events {

typedef std::function<void(void)> WorkItem;

class EventQueue {
 public:
  static EventQueue& GetInstance() {
    static EventQueue instance;
    return instance;
  }

  template <typename Event, typename Machine, typename... Machines>
  auto Dispatch(const Event& ev) -> void {
    WorkItem* item = new WorkItem(
        [=]() { tinyfsm::FsmList<Machine>::template dispatch<Event>(ev); });
    if (std::is_same<Machine, ui::UiState>()) {
      xQueueSend(system_handle_, &item, portMAX_DELAY);
    } else {
      xQueueSend(ui_handle_, &item, portMAX_DELAY);
    }
    Dispatch<Event, Machines...>(ev);
  }

  template <typename Event>
  auto Dispatch(const Event& ev) -> void {}

  auto ServiceSystem(TickType_t max_wait_time) -> bool;
  auto ServiceUi(TickType_t max_wait_time) -> bool;

  EventQueue(EventQueue const&) = delete;
  void operator=(EventQueue const&) = delete;

 private:
  EventQueue();

  QueueHandle_t system_handle_;
  QueueHandle_t ui_handle_;
};

template <typename Event, typename... Machines>
auto Dispatch(const Event& ev) -> void {
  EventQueue& queue = EventQueue::GetInstance();
  queue.Dispatch<Event, Machines...>(ev);
}

}  // namespace events
