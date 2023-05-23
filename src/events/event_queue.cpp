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
    : handle_(xQueueCreate(kMaxPendingEvents, sizeof(WorkItem*))) {}

auto EventQueue::Service(TickType_t max_wait_time) -> bool {
  WorkItem* item;
  if (xQueueReceive(handle_, &item, max_wait_time)) {
    (*item)();
    delete item;
    return true;
  }
  return false;
}

}  // namespace events
