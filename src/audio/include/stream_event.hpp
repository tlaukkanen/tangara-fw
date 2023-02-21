#pragma once

#include <memory>

#include "arena.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "span.hpp"
#include "stream_info.hpp"

namespace audio {

struct StreamEvent {
  static auto CreateStreamInfo(QueueHandle_t source, const StreamInfo& payload)
      -> StreamEvent*;
  static auto CreateArenaChunk(QueueHandle_t source, memory::ArenaPtr ptr)
      -> StreamEvent*;
  static auto CreateChunkNotification(QueueHandle_t source) -> StreamEvent*;
  static auto CreateEndOfStream(QueueHandle_t source) -> StreamEvent*;
  static auto CreateLogStatus() -> StreamEvent*;

  StreamEvent();
  ~StreamEvent();
  StreamEvent(StreamEvent&&);

  QueueHandle_t source;

  enum {
    UNINITIALISED,
    STREAM_INFO,
    ARENA_CHUNK,
    CHUNK_NOTIFICATION,
    END_OF_STREAM,
    LOG_STATUS,
  } tag;

  union {
    StreamInfo* stream_info;

    memory::ArenaPtr arena_chunk;

    // FIXME: It would be nice to also support a pointer to himem data here, to
    // save a little ordinary heap space.
  };

  StreamEvent(const StreamEvent&) = delete;
  StreamEvent& operator=(const StreamEvent&) = delete;
};

}  // namespace audio
