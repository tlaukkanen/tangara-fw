#pragma once

#include <memory>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "span.hpp"
#include "stream_info.hpp"

namespace audio {

struct StreamEvent {
  static auto CreateStreamInfo(QueueHandle_t source,
                               std::unique_ptr<StreamInfo> payload)
      -> std::unique_ptr<StreamEvent>;
  static auto CreateChunkData(QueueHandle_t source, std::size_t chunk_size)
      -> std::unique_ptr<StreamEvent>;
  static auto CreateChunkNotification(QueueHandle_t source)
      -> std::unique_ptr<StreamEvent>;

  StreamEvent();
  ~StreamEvent();
  StreamEvent(StreamEvent&&);

  QueueHandle_t source;

  enum {
    UNINITIALISED,
    STREAM_INFO,
    CHUNK_DATA,
    CHUNK_NOTIFICATION,
  } tag;

  union {
    std::unique_ptr<StreamInfo> stream_info;

    // Scott Meyers says:
    // `About the only situation I can conceive of when a std::unique_ptr<T[]>
    // would make sense would be when you’re using a C-like API that returns a
    // raw pointer to a heap array that you assume ownership of.`
    // :-)

    struct {
      std::unique_ptr<std::byte*> raw_bytes;
      cpp::span<std::byte> bytes;
    } chunk_data;

    // FIXME: It would be nice to also support a pointer to himem data here, to
    // save a little ordinary heap space.
  };

  StreamEvent(const StreamEvent&) = delete;
  StreamEvent& operator=(const StreamEvent&) = delete;
};

}  // namespace audio
