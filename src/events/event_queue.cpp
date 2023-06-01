/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "event_queue.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace events {

static const std::size_t kMaxPendingEvents = 16;

EventQueue::EventQueue()
    : system_handle_(xQueueCreate(kMaxPendingEvents, sizeof(WorkItem*))),
      ui_handle_(xQueueCreate(kMaxPendingEvents, sizeof(WorkItem*))),
      audio_handle_(xQueueCreate(kMaxPendingEvents, sizeof(WorkItem*))) {}

auto EventQueue::ServiceSystem(TickType_t max_wait_time) -> bool {
  WorkItem* item;
  if (xQueueReceive(system_handle_, &item, max_wait_time)) {
    (*item)();
    delete item;
    return true;
  }
  return false;
}

auto EventQueue::ServiceUi(TickType_t max_wait_time) -> bool {
  WorkItem* item;
  if (xQueueReceive(ui_handle_, &item, max_wait_time)) {
    (*item)();
    delete item;
    return true;
  }
  return false;
}

}  // namespace events
