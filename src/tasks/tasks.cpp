#include "tasks.hpp"
#include <functional>
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

namespace tasks {

template <Type t>
auto Name() -> std::string;

template <>
auto Name<Type::kUi>() -> std::string {
  return "LVGL";
}
template <>
auto Name<Type::kUiFlush>() -> std::string {
  return "DISPLAY";
}
template <>
auto Name<Type::kAudio>() -> std::string {
  return "AUDIO";
}
template <>
auto Name<Type::kAudioDrain>() -> std::string {
  return "DRAIN";
}
template <>
auto Name<Type::kDatabase>() -> std::string {
  return "DB";
}

template <Type t>
auto AllocateStack() -> cpp::span<StackType_t>;

// Decoders run on the audio task, and these sometimes require a fairly large
// amount of stack space.
template <>
auto AllocateStack<Type::kAudio>() -> cpp::span<StackType_t> {
  std::size_t size = 32 * 1024;
  return {static_cast<StackType_t*>(heap_caps_malloc(size, MALLOC_CAP_DEFAULT)),
          size};
}
template <>
auto AllocateStack<Type::kAudioDrain>() -> cpp::span<StackType_t> {
  std::size_t size = 1024;
  return {static_cast<StackType_t*>(heap_caps_malloc(size, MALLOC_CAP_DEFAULT)),
          size};
}
// LVGL requires only a relatively small stack. However, it can be allocated in
// PSRAM so we give it a bit of headroom for safety.
template <>
auto AllocateStack<Type::kUi>() -> cpp::span<StackType_t> {
  std::size_t size = 16 * 1024;
  return {static_cast<StackType_t*>(heap_caps_malloc(size, MALLOC_CAP_DEFAULT)),
          size};
}
// UI flushes *must* be done from internal RAM. Thankfully, there is very little
// stack required to perform them, and the amount of stack needed is fixed.
template <>
auto AllocateStack<Type::kUiFlush>() -> cpp::span<StackType_t> {
  std::size_t size = 1024;
  return {static_cast<StackType_t*>(heap_caps_malloc(size, MALLOC_CAP_DEFAULT)),
          size};
}
// Leveldb is designed for non-embedded use cases, where stack space isn't so
// much of a concern. It therefore uses an eye-wateringly large amount of stack.
template <>
auto AllocateStack<Type::kDatabase>() -> cpp::span<StackType_t> {
  std::size_t size = 256 * 1024;
  return {static_cast<StackType_t*>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM)),
          size};
}

// 2048 bytes in internal ram
// 302 KiB in external ram.

/*
 * Please keep the priorities below in descending order for better readability.
 */

template <Type t>
auto Priority() -> UBaseType_t;

// Realtime audio is the entire point of this device, so give this task the
// highest priority.
template <>
auto Priority<Type::kAudio>() -> UBaseType_t {
  return 10;
}
template <>
auto Priority<Type::kAudioDrain>() -> UBaseType_t {
  return 10;
}
// After audio issues, UI jank is the most noticeable kind of scheduling-induced
// slowness that the user is likely to notice or care about. Therefore we place
// this task directly below audio in terms of priority.
template <>
auto Priority<Type::kUi>() -> UBaseType_t {
  return 9;
}
// UI flushing should use the same priority as the UI task, so as to maximise
// the chance of the happy case: one of our cores is writing to the screen,
// whilst the other is simultaneously preparing the next buffer to be flushed.
template <>
auto Priority<Type::kUiFlush>() -> UBaseType_t {
  return 9;
}
// Database interactions are all inherently async already, due to their
// potential for disk access. The user likely won't notice or care about a
// couple of ms extra delay due to scheduling, so give this task the lowest
// priority.
template <>
auto Priority<Type::kDatabase>() -> UBaseType_t {
  return 8;
}

template <Type t>
auto WorkerQueueSize() -> std::size_t;

template <>
auto WorkerQueueSize<Type::kDatabase>() -> std::size_t {
  return 8;
}

template <>
auto WorkerQueueSize<Type::kUiFlush>() -> std::size_t {
  return 2;
}

auto PersistentMain(void* fn) -> void {
  auto* function = reinterpret_cast<std::function<void(void)>*>(fn);
  std::invoke(*function);
  assert("persistent task quit!" == 0);
  vTaskDelete(NULL);
}

auto Worker::Main(void* instance) {
  Worker* i = reinterpret_cast<Worker*>(instance);
  while (1) {
    WorkItem item;
    if (xQueueReceive(i->queue_, &item, portMAX_DELAY)) {
      if (item.quit) {
        break;
      } else if (item.fn != nullptr) {
        std::invoke(*item.fn);
        delete item.fn;
      }
    }
  }
  i->is_task_running_.store(false);
  i->is_task_running_.notify_all();
  // Wait for the instance's destructor to delete this task. We do this instead
  // of just deleting ourselves so that it's 100% certain that it's safe to
  // delete or reuse this task's stack.
  while (1) {
    vTaskDelay(portMAX_DELAY);
  }
}

Worker::Worker(const std::string& name,
               cpp::span<StackType_t> stack,
               std::size_t queue_size,
               UBaseType_t priority)
    : stack_(stack.data()),
      queue_(xQueueCreate(queue_size, sizeof(WorkItem))),
      is_task_running_(true),
      task_buffer_(),
      task_(xTaskCreateStatic(&Main,
                              name.c_str(),
                              stack.size(),
                              this,
                              priority,
                              stack_,
                              &task_buffer_)) {}

Worker::~Worker() {
  WorkItem item{
      .fn = nullptr,
      .quit = true,
  };
  xQueueSend(queue_, &item, portMAX_DELAY);
  is_task_running_.wait(true);
  vTaskDelete(task_);
  free(stack_);
}

template <>
auto Worker::Dispatch(const std::function<void(void)>& fn)
    -> std::future<void> {
  std::shared_ptr<std::promise<void>> promise =
      std::make_shared<std::promise<void>>();
  WorkItem item{
      .fn = new std::function<void(void)>([=]() {
        std::invoke(fn);
        promise->set_value();
      }),
      .quit = false,
  };
  xQueueSend(queue_, &item, portMAX_DELAY);
  return promise->get_future();
}

}  // namespace tasks
