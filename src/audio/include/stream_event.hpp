#pragma once

#include <memory>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "span.hpp"
#include "stream_info.hpp"

namespace audio {

struct StreamEvent {
  static auto CreateStreamInfo(QueueHandle_t source, const StreamInfo& payload)
      -> StreamEvent*;
  static auto CreateChunkData(QueueHandle_t source, std::size_t chunk_size)
      -> StreamEvent*;
  static auto CreateChunkNotification(QueueHandle_t source) -> StreamEvent*;
  static auto CreateEndOfStream(QueueHandle_t source) -> StreamEvent*;

  StreamEvent();
  ~StreamEvent();
  StreamEvent(StreamEvent&&);

  QueueHandle_t source;

  enum {
    UNINITIALISED,
    STREAM_INFO,
    CHUNK_DATA,
    CHUNK_NOTIFICATION,
    END_OF_STREAM,
  } tag;

  union {
    StreamInfo* stream_info;

    struct {
      std::byte* raw_bytes;
      cpp::span<std::byte> bytes;
    } chunk_data;

    // FIXME: It would be nice to also support a pointer to himem data here, to
    // save a little ordinary heap space.
  };

  StreamEvent(const StreamEvent&) = delete;
  StreamEvent& operator=(const StreamEvent&) = delete;
};

}  // namespace audio
