#include "db_task.hpp"

#include <functional>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/projdefs.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace database {

static const std::size_t kDbStackSize = 256 * 1024;
static StaticTask_t sDbStaticTask;
static StackType_t* sDbStack = nullptr;

static std::atomic<bool> sTaskRunning(false);
static QueueHandle_t sWorkQueue;

struct WorkItem {
  std::function<void(void)>* fn;
  bool quit;
};

auto SendToDbTask(std::function<void(void)> fn) -> void {
  WorkItem item{
      .fn = new std::function<void(void)>(fn),
      .quit = false,
  };
  xQueueSend(sWorkQueue, &item, portMAX_DELAY);
}

template <>
auto RunOnDbTask(std::function<void(void)> fn) -> std::future<void> {
  std::shared_ptr<std::promise<void>> promise =
      std::make_shared<std::promise<void>>();
  SendToDbTask([=]() {
    std::invoke(fn);
    promise->set_value();
  });
  return promise->get_future();
}

void DatabaseTaskMain(void* args) {
  while (true) {
    WorkItem item;
    if (xQueueReceive(sWorkQueue, &item, portMAX_DELAY)) {
      if (item.fn != nullptr) {
        std::invoke(*item.fn);
        delete item.fn;
      }
      if (item.quit) {
        break;
      }
    }
  }
  vQueueDelete(sWorkQueue);
  sTaskRunning.store(false);
  vTaskDelete(NULL);
}

auto StartDbTask() -> bool {
  if (sTaskRunning.exchange(true)) {
    return false;
  }
  if (sDbStack == nullptr) {
    sDbStack = reinterpret_cast<StackType_t*>(
        heap_caps_malloc(kDbStackSize, MALLOC_CAP_SPIRAM));
  }
  sWorkQueue = xQueueCreate(8, sizeof(WorkItem));
  xTaskCreateStatic(&DatabaseTaskMain, "DB", kDbStackSize, NULL, 1, sDbStack,
                    &sDbStaticTask);
  return true;
}

auto QuitDbTask() -> void {
  if (!sTaskRunning.load()) {
    return;
  }
  WorkItem item{
      .fn = nullptr,
      .quit = true,
  };
  xQueueSend(sWorkQueue, &item, portMAX_DELAY);
  while (sTaskRunning.load()) {
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

}  // namespace database
