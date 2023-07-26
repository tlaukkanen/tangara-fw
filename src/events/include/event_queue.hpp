/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <functional>
#include <mutex>
#include <queue>
#include <type_traits>

#include "audio_fsm.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "system_fsm.hpp"
#include "tinyfsm.hpp"

#include "ui_fsm.hpp"

namespace events {

class Queue {
 public:
  Queue() : has_events_(xSemaphoreCreateBinary()), mut_(), events_() {}

  auto Add(std::function<void(void)> fn) {
    {
      std::lock_guard<std::mutex> lock{mut_};
      events_.push(fn);
    }
    xSemaphoreGive(has_events_);
  }

  auto Service(TickType_t max_wait) -> bool {
    bool res = xSemaphoreTake(has_events_, max_wait);
    if (!res) {
      return false;
    }

    bool had_work = false;
    for (;;) {
      std::function<void(void)> fn;
      {
        std::lock_guard<std::mutex> lock{mut_};
        if (events_.empty()) {
          return had_work;
        }
        had_work = true;
        fn = events_.front();
        events_.pop();
      }
      std::invoke(fn);
    }
  }

  auto has_events() -> SemaphoreHandle_t { return has_events_; }

  Queue(Queue const&) = delete;
  void operator=(Queue const&) = delete;

 private:
  SemaphoreHandle_t has_events_;
  std::mutex mut_;
  std::queue<std::function<void(void)>> events_;
};

template <class Machine>
class Dispatcher {
 public:
  Dispatcher(Queue* queue) : queue_(queue) {}

  template <typename Event>
  auto Dispatch(const Event& ev) -> void {
    auto dispatch_fn = [=]() {
      tinyfsm::FsmList<Machine>::template dispatch<Event>(ev);
    };
    queue_->Add(dispatch_fn);
  }

  Dispatcher(Dispatcher const&) = delete;
  void operator=(Dispatcher const&) = delete;

 private:
  Queue* queue_;
};

namespace queues {
auto SystemAndAudio() -> Queue*;
auto Ui() -> Queue*;
}  // namespace queues

auto System() -> Dispatcher<system_fsm::SystemState>&;
auto Audio() -> Dispatcher<audio::AudioState>&;
auto Ui() -> Dispatcher<ui::UiState>&;

}  // namespace events
