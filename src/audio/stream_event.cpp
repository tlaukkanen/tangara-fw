/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "stream_event.hpp"
#include <cstddef>
#include <memory>
#include "arena.hpp"
#include "stream_info.hpp"

namespace audio {

auto StreamEvent::CreateStreamInfo(QueueHandle_t source,
                                   const StreamInfo& payload) -> StreamEvent* {
  auto event = new StreamEvent;
  event->tag = StreamEvent::STREAM_INFO;
  event->source = source;
  event->stream_info = new StreamInfo(payload);
  return event;
}

auto StreamEvent::CreateArenaChunk(QueueHandle_t source, memory::ArenaPtr ptr)
    -> StreamEvent* {
  auto event = new StreamEvent;
  event->tag = StreamEvent::ARENA_CHUNK;
  event->source = source;
  event->arena_chunk = ptr;

  return event;
}

auto StreamEvent::CreateChunkNotification(QueueHandle_t source)
    -> StreamEvent* {
  auto event = new StreamEvent;
  event->tag = StreamEvent::CHUNK_NOTIFICATION;
  event->source = source;
  return event;
}

auto StreamEvent::CreateEndOfStream(QueueHandle_t source) -> StreamEvent* {
  auto event = new StreamEvent;
  event->tag = StreamEvent::END_OF_STREAM;
  event->source = source;
  return event;
}

auto StreamEvent::CreateLogStatus() -> StreamEvent* {
  auto event = new StreamEvent;
  event->tag = StreamEvent::LOG_STATUS;
  return event;
}

StreamEvent::StreamEvent() : tag(StreamEvent::UNINITIALISED) {}

StreamEvent::~StreamEvent() {
  switch (tag) {
    case UNINITIALISED:
      break;
    case STREAM_INFO:
      delete stream_info;
      break;
    case ARENA_CHUNK:
      arena_chunk.owner->Return(arena_chunk);
      break;
    case CHUNK_NOTIFICATION:
      break;
    case END_OF_STREAM:
      break;
    case LOG_STATUS:
      break;
  }
}

StreamEvent::StreamEvent(StreamEvent&& other) {
  tag = other.tag;
  source = other.source;
  switch (tag) {
    case UNINITIALISED:
      break;
    case STREAM_INFO:
      stream_info = other.stream_info;
      other.stream_info = nullptr;
      break;
    case ARENA_CHUNK:
      arena_chunk = other.arena_chunk;
      other.arena_chunk = {
          .owner = nullptr, .start = nullptr, .size = 0, .used_size = 0};
      break;
    case CHUNK_NOTIFICATION:
      break;
    case END_OF_STREAM:
      break;
    case LOG_STATUS:
      break;
  }
  other.tag = StreamEvent::UNINITIALISED;
}

}  // namespace audio
