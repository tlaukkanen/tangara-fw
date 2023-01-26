#include "stream_event.hpp"
#include <cstddef>
#include <memory>

namespace audio {

auto StreamEvent::CreateStreamInfo(QueueHandle_t source,
                                   std::unique_ptr<StreamInfo> payload)
    -> std::unique_ptr<StreamEvent> {
  auto event = std::make_unique<StreamEvent>();
  event->tag = StreamEvent::STREAM_INFO;
  event->source = source;
  event->stream_info = std::move(payload);
  return event;
}

auto StreamEvent::CreateChunkData(QueueHandle_t source, std::size_t chunk_size)
    -> std::unique_ptr<StreamEvent> {
  auto event = std::make_unique<StreamEvent>();
  event->tag = StreamEvent::CHUNK_DATA;
  event->source = source;

  auto raw_bytes =
      static_cast<std::byte*>(heap_caps_malloc(chunk_size, MALLOC_CAP_SPIRAM));

  event->chunk_data.raw_bytes = std::make_unique<std::byte*>(raw_bytes);
  event->chunk_data.bytes = cpp::span<std::byte>(raw_bytes, chunk_size);

  return event;
}

auto StreamEvent::CreateChunkNotification(QueueHandle_t source)
    -> std::unique_ptr<StreamEvent> {
  auto event = std::make_unique<StreamEvent>();
  event->tag = StreamEvent::CHUNK_NOTIFICATION;
  event->source = source;
  return event;
}

StreamEvent::StreamEvent() : tag(StreamEvent::UNINITIALISED) {}

StreamEvent::~StreamEvent() {
  switch (tag) {
    case UNINITIALISED:
      break;
    case STREAM_INFO:
      stream_info.reset();
      break;
    case CHUNK_DATA:
      chunk_data.raw_bytes.reset();
      break;
    case CHUNK_NOTIFICATION:
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
      stream_info = std::move(other.stream_info);
      break;
    case CHUNK_DATA:
      chunk_data = std::move(other.chunk_data);
      break;
    case CHUNK_NOTIFICATION:
      break;
  }
  other.tag = StreamEvent::UNINITIALISED;
}

}  // namespace audio
