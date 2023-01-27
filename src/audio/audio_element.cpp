#include "audio_element.hpp"

namespace audio {

IAudioElement::IAudioElement()
    : input_events_(xQueueCreate(kEventQueueSize, sizeof(void*))),
      output_events_(nullptr),
      unprocessed_output_chunks_(0),
      buffered_output_(),
      current_state_(STATE_RUN) {}

IAudioElement::~IAudioElement() {
  // Ensure we don't leak any memory from events leftover in the queue.
  while (uxQueueSpacesAvailable(input_events_) < kEventQueueSize) {
    StreamEvent* event;
    if (xQueueReceive(input_events_, &event, 0)) {
      free(event);
    } else {
      break;
    }
  }
  // Technically there's a race here if someone is still adding to the queue,
  // but hopefully the whole pipeline is stopped if an element is being
  // destroyed.
  vQueueDelete(input_events_);
}

auto IAudioElement::SendOrBufferEvent(std::unique_ptr<StreamEvent> event)
    -> bool {
  if (event->tag == StreamEvent::CHUNK_DATA) {
    unprocessed_output_chunks_++;
  }
  if (!buffered_output_.empty()) {
    // To ensure we send data in order, don't try to send if we've already
    // failed to send something.
    buffered_output_.push_back(std::move(event));
    return false;
  }
  StreamEvent* raw_event = event.release();
  if (!xQueueSend(output_events_, &raw_event, 0)) {
    buffered_output_.emplace_front(raw_event);
    return false;
  }
  return true;
}

auto IAudioElement::FlushBufferedOutput() -> bool {
  while (!buffered_output_.empty()) {
    StreamEvent* raw_event = buffered_output_.front().release();
    buffered_output_.pop_front();
    if (!xQueueSend(output_events_, &raw_event, 0)) {
      buffered_output_.emplace_front(raw_event);
      return false;
    }
  }
  return true;
}

}  // namespace audio
