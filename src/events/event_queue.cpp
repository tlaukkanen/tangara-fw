/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "event_queue.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"

namespace events {

static const std::size_t kMaxPendingEvents = 16;

EventQueue::EventQueue()
    : system_handle_(xQueueCreate(kMaxPendingEvents, sizeof(WorkItem*))),
      ui_handle_(xQueueCreate(kMaxPendingEvents, sizeof(WorkItem*))),
      audio_handle_(xQueueCreate(kMaxPendingEvents, sizeof(WorkItem*))) {}

auto ServiceQueue(QueueHandle_t queue, TickType_t max_wait_time) -> bool {
  WorkItem* item;
  if (xQueueReceive(queue, &item, max_wait_time)) {
    (*item)();
    delete item;
    return true;
  }
  return false;
}

auto EventQueue::ServiceSystem(TickType_t max_wait_time) -> bool {
  return ServiceQueue(system_handle_, max_wait_time);
}

auto EventQueue::ServiceUi(TickType_t max_wait_time) -> bool {
  return ServiceQueue(ui_handle_, max_wait_time);
}

auto EventQueue::ServiceAudio(TickType_t max_wait_time) -> bool {
  return ServiceQueue(audio_handle_, max_wait_time);
}

}  // namespace events
