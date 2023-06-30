/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <functional>
#include <type_traits>

#include "audio_fsm.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "system_fsm.hpp"
#include "tinyfsm.hpp"

#include "ui_fsm.hpp"

namespace events {

typedef std::function<void(void)> WorkItem;

/*
 * Handles communication of events between the system's state machines. Each
 * event will be dispatched separately to each FSM, on the correct task for
 * that FSM.
 */
class EventQueue {
 public:
  static EventQueue& GetInstance() {
    static EventQueue instance;
    return instance;
  }

  template <typename Event>
  auto DispatchFromISR(const Event& ev) -> bool {
    WorkItem* item = new WorkItem([=]() {
      tinyfsm::FsmList<system_fsm::SystemState>::template dispatch<Event>(ev);
    });
    BaseType_t ret;
    xQueueSendFromISR(system_handle_, &item, &ret);
    return ret;
  }

  template <typename Event, typename Machine, typename... Machines>
  auto Dispatch(const Event& ev) -> void {
    WorkItem* item = new WorkItem(
        [=]() { tinyfsm::FsmList<Machine>::template dispatch<Event>(ev); });
    if (std::is_same<Machine, ui::UiState>()) {
      xQueueSend(ui_handle_, &item, portMAX_DELAY);
    } else if (std::is_same<Machine, audio::AudioState>()) {
      xQueueSend(audio_handle_, &item, portMAX_DELAY);
    } else {
      xQueueSend(system_handle_, &item, portMAX_DELAY);
    }
    Dispatch<Event, Machines...>(ev);
  }

  template <typename Event>
  auto Dispatch(const Event& ev) -> void {}

  auto ServiceSystem(TickType_t max_wait_time) -> bool;
  auto ServiceUi(TickType_t max_wait_time) -> bool;
  auto ServiceAudio(TickType_t max_wait_time) -> bool;

  EventQueue(EventQueue const&) = delete;
  void operator=(EventQueue const&) = delete;

 private:
  EventQueue();

  QueueHandle_t system_handle_;
  QueueHandle_t ui_handle_;
  QueueHandle_t audio_handle_;
};

template <typename Event, typename... Machines>
auto Dispatch(const Event& ev) -> void {
  EventQueue& queue = EventQueue::GetInstance();
  queue.Dispatch<Event, Machines...>(ev);
}

}  // namespace events
