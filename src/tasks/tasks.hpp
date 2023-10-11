/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <string>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "span.hpp"

namespace tasks {

/*
 * Enumeration of every task (basically a thread) started within the firmware.
 * These are centralised so that it is easier to reason about the relative
 * priorities of tasks, as well as the amount and location of memory allocated
 * to each one.
 */
enum class Type {
  // The main UI task. This runs the LVGL main loop.
  kUi,
  // The main audio pipeline task. Decodes files into PCM stream.
  kAudioDecoder,
  // Second audio task. Converts the PCM stream into one suitable for the
  // current output (e.g. downsampling for bluetooth).
  kAudioConverter,
  // Task for running database queries.
  kDatabase,
  // Task for internal database operations
  kDatabaseBackground,
};

template <Type t>
auto Name() -> std::pmr::string;
template <Type t>
auto AllocateStack() -> cpp::span<StackType_t>;
template <Type t>
auto Priority() -> UBaseType_t;
template <Type t>
auto WorkerQueueSize() -> std::size_t;

auto PersistentMain(void* fn) -> void;

template <Type t>
auto StartPersistent(const std::function<void(void)>& fn) -> void {
  StaticTask_t* task_buffer = static_cast<StaticTask_t*>(heap_caps_malloc(
      sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  cpp::span<StackType_t> stack = AllocateStack<t>();
  xTaskCreateStatic(&PersistentMain, Name<t>().c_str(), stack.size(),
                    new std::function<void(void)>(fn), Priority<t>(),
                    stack.data(), task_buffer);
}

template <Type t>
auto StartPersistent(BaseType_t core, const std::function<void(void)>& fn)
    -> void {
  StaticTask_t* task_buffer = static_cast<StaticTask_t*>(heap_caps_malloc(
      sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  cpp::span<StackType_t> stack = AllocateStack<t>();
  xTaskCreateStaticPinnedToCore(&PersistentMain, Name<t>().c_str(),
                                stack.size(), new std::function<void(void)>(fn),
                                Priority<t>(), stack.data(), task_buffer, core);
}

class Worker {
 private:
  Worker(const std::pmr::string& name,
         cpp::span<StackType_t> stack,
         std::size_t queue_size,
         UBaseType_t priority);

  StackType_t* stack_;
  QueueHandle_t queue_;
  std::atomic<bool> is_task_running_;
  StaticTask_t *task_buffer_;
  TaskHandle_t task_;

  struct WorkItem {
    std::function<void(void)>* fn;
    bool quit;
  };

 public:
  template <Type t>
  static auto Start() -> Worker* {
    return new Worker(Name<t>(), AllocateStack<t>(), WorkerQueueSize<t>(),
                      Priority<t>());
  }

  static auto Main(void* instance);

  /*
   * Schedules the given function to be executed on the worker task, and
   * asynchronously returns the result as a future.
   */
  template <typename T>
  auto Dispatch(const std::function<T(void)>& fn) -> std::future<T> {
    std::shared_ptr<std::promise<T>> promise =
        std::make_shared<std::promise<T>>();
    WorkItem item{
        .fn = new std::function([=]() { promise->set_value(std::invoke(fn)); }),
        .quit = false,
    };
    xQueueSend(queue_, &item, portMAX_DELAY);
    return promise->get_future();
  }

  ~Worker();

  Worker(const Worker&) = delete;
  Worker& operator=(const Worker&) = delete;
};

/* Specialisation of Evaluate for functions that return nothing. */
template <>
auto Worker::Dispatch(const std::function<void(void)>& fn) -> std::future<void>;

}  // namespace tasks
