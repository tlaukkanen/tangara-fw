/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "tasks.hpp"

#include <functional>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

#include "memory_resource.hpp"

namespace tasks {

template <Type t>
auto Name() -> std::pmr::string;

template <>
auto Name<Type::kUi>() -> std::pmr::string {
  return "ui";
}
template <>
auto Name<Type::kAudioDecoder>() -> std::pmr::string {
  return "audio_dec";
}
template <>
auto Name<Type::kAudioConverter>() -> std::pmr::string {
  return "audio_conv";
}

template <Type t>
auto AllocateStack() -> cpp::span<StackType_t>;

// Decoders often require a very large amount of stack space, since they aren't
// usually written with embedded use cases in mind.
template <>
auto AllocateStack<Type::kAudioDecoder>() -> cpp::span<StackType_t> {
  constexpr std::size_t size = 24 * 1024;
  static StackType_t sStack[size];
  return {sStack, size};
}
// LVGL requires only a relatively small stack. However, it can be allocated in
// PSRAM so we give it a bit of headroom for safety.
template <>
auto AllocateStack<Type::kUi>() -> cpp::span<StackType_t> {
  constexpr std::size_t size = 16 * 1024;
  static StackType_t sStack[size];
  return {sStack, size};
}
template <>
// PCM conversion and resampling uses a very small amount of stack. It works
// entirely with PSRAM-allocated buffers, so no real speed gain from allocating
// it internally.
auto AllocateStack<Type::kAudioConverter>() -> cpp::span<StackType_t> {
  constexpr std::size_t size = 4 * 1024;
  static StackType_t sStack[size];
  return {sStack, size};
}
// Background workers receive huge stacks in PSRAM. This is mostly to faciliate
// use of LevelDB from any bg worker; Leveldb is designed for non-embedded use
// cases, where large stack usage isn't so much of a concern. It therefore uses
// an eye-wateringly large amount of stack.
template <>
auto AllocateStack<Type::kBackgroundWorker>() -> cpp::span<StackType_t> {
  std::size_t size = 256 * 1024;
  return {static_cast<StackType_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM)),
          size};
}

// 2 KiB in internal ram
// 612 KiB in external ram.

/*
 * Please keep the priorities below in descending order for better readability.
 */

template <Type t>
auto Priority() -> UBaseType_t;

// Realtime audio is the entire point of this device, so give these tasks the
// highest priority.
template <>
auto Priority<Type::kAudioDecoder>() -> UBaseType_t {
  return configMAX_PRIORITIES - 1;
}
template <>
auto Priority<Type::kAudioConverter>() -> UBaseType_t {
  return configMAX_PRIORITIES - 1;
}
// After audio issues, UI jank is the most noticeable kind of scheduling-induced
// slowness that the user is likely to notice or care about. Therefore we place
// this task directly below audio in terms of priority.
template <>
auto Priority<Type::kUi>() -> UBaseType_t {
  return 10;
}
// Database interactions are all inherently async already, due to their
// potential for disk access. The user likely won't notice or care about a
// couple of ms extra delay due to scheduling, so give this task the lowest
// priority.
template <>
auto Priority<Type::kBackgroundWorker>() -> UBaseType_t {
  return 1;
}

auto PersistentMain(void* fn) -> void {
  auto* function = reinterpret_cast<std::function<void(void)>*>(fn);
  std::invoke(*function);
  assert("persistent task quit!" == 0);
  vTaskDelete(NULL);
}

auto WorkerPool::Main(void* q) {
  QueueHandle_t queue = reinterpret_cast<QueueHandle_t>(q);
  while (1) {
    WorkItem item;
    if (xQueueReceive(queue, &item, portMAX_DELAY)) {
      std::invoke(*item);
      delete item;
    }
  }
}

static constexpr size_t kNumWorkers = 3;
static constexpr size_t kMaxPendingItems = 8;

WorkerPool::WorkerPool()
    : queue_(xQueueCreate(kMaxPendingItems, sizeof(WorkItem))) {
  for (size_t i = 0; i < kNumWorkers; i++) {
    auto stack = AllocateStack<Type::kBackgroundWorker>();
    // Task buffers must be in internal ram. Thankfully they're fairly small.
    auto buffer = reinterpret_cast<StaticTask_t*>(heap_caps_malloc(
        sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));

    std::string name = "worker_" + std::to_string(i);

    xTaskCreateStatic(&Main, name.c_str(), stack.size(), queue_,
                      Priority<Type::kBackgroundWorker>(), stack.data(),
                      buffer);
  }
}

WorkerPool::~WorkerPool() {
  // This should never happen!
  assert("worker pool destroyed" == 0);
}

template <>
auto WorkerPool::Dispatch(const std::function<void(void)> fn)
    -> std::future<void> {
  std::shared_ptr<std::promise<void>> promise =
      std::make_shared<std::promise<void>>();
  WorkItem item = new std::function<void(void)>([=]() {
    std::invoke(fn);
    promise->set_value();
  });
  xQueueSend(queue_, &item, portMAX_DELAY);
  return promise->get_future();
}

}  // namespace tasks
