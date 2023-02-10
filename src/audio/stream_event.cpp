#include "stream_event.hpp"
#include <cstddef>
#include <memory>
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

auto StreamEvent::CreateChunkData(QueueHandle_t source, std::size_t chunk_size)
    -> StreamEvent* {
  auto event = new StreamEvent;
  event->tag = StreamEvent::CHUNK_DATA;
  event->source = source;

  auto raw_bytes =
      static_cast<std::byte*>(heap_caps_malloc(chunk_size, MALLOC_CAP_SPIRAM));

  event->chunk_data.raw_bytes = raw_bytes;
  event->chunk_data.bytes = cpp::span<std::byte>(raw_bytes, chunk_size);

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

StreamEvent::StreamEvent() : tag(StreamEvent::UNINITIALISED) {}

StreamEvent::~StreamEvent() {
  switch (tag) {
    case UNINITIALISED:
      break;
    case STREAM_INFO:
      delete stream_info;
      break;
    case CHUNK_DATA:
      free(chunk_data.raw_bytes);
      break;
    case CHUNK_NOTIFICATION:
      break;
    case END_OF_STREAM:
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
    case CHUNK_DATA:
      chunk_data = other.chunk_data;
      other.chunk_data = {};
      break;
    case CHUNK_NOTIFICATION:
      break;
    case END_OF_STREAM:
      break;
  }
  other.tag = StreamEvent::UNINITIALISED;
}

}  // namespace audio
