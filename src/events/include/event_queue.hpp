/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <functional>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "tinyfsm.hpp"

namespace events {

typedef std::function<void(void)> WorkItem;

class EventQueue {
 public:
  static EventQueue& GetInstance() {
    static EventQueue instance;
    return instance;
  }

  template <typename Event, typename... Machines>
  auto Dispatch(const Event& ev) -> void {
    WorkItem* item = new WorkItem(
        [=]() { tinyfsm::FsmList<Machines...>::template dispatch<Event>(ev); });
    xQueueSend(handle_, &item, portMAX_DELAY);
  }

  auto Service(TickType_t max_wait_time) -> bool;

  EventQueue(EventQueue const&) = delete;
  void operator=(EventQueue const&) = delete;

 private:
  EventQueue();

  QueueHandle_t handle_;
};

template <typename Event, typename... Machines>
auto Dispatch(const Event& ev) -> void {
  EventQueue& queue = EventQueue::GetInstance();
  queue.Dispatch<Event, Machines...>(ev);
}

}  // namespace events
