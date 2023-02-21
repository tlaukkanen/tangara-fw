#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>

#include "freertos/FreeRTOS.h"

#include "chunk.hpp"
#include "freertos/message_buffer.h"
#include "freertos/portmacro.h"
#include "result.hpp"
#include "span.hpp"

#include "stream_buffer.hpp"
#include "stream_event.hpp"
#include "stream_info.hpp"
#include "types.hpp"

namespace audio {

static const size_t kEventQueueSize = 8;

/*
 * One indepedentent part of an audio pipeline. Each element has an input and
 * output message stream, and is responsible for taking data from the input
 * stream, applying some kind of transformation to it, then sending the result
 * out via the output stream. All communication with an element should be done
 * over these streams, as an element's methods are only safe to call from the
 * task that owns it.
 *
 * Each element implementation will have its input stream automatically parsed
 * by its owning task, and its various Process* methods will be invoked
 * accordingly. Element implementations are responsible for managing their own
 * writes to their output streams.
 */
class IAudioElement {
 public:
  IAudioElement();
  virtual ~IAudioElement();

  /*
   * Returns the stack size in bytes that this element requires. This should
   * be tuned according to the observed stack size of each element, as different
   * elements have fairly different stack requirements (particular decoders).
   */
  virtual auto StackSizeBytes() const -> std::size_t { return 4096; };

  /* Returns this element's input buffer. */
  auto InputEventQueue() const -> QueueHandle_t { return input_events_; }
  /* Returns this element's output buffer. */
  auto OutputEventQueue() const -> QueueHandle_t { return output_events_; }
  auto OutputEventQueue(const QueueHandle_t q) -> void { output_events_ = q; }

  virtual auto HasUnprocessedInput() -> bool = 0;

  virtual auto IsOverBuffered() -> bool { return false; }

  auto HasUnflushedOutput() -> bool { return !buffered_output_.empty(); }
  auto FlushBufferedOutput() -> bool;

  /*
   * Called when a StreamInfo message is received. Used to configure this
   * element in preperation for incoming chunks.
   */
  virtual auto ProcessStreamInfo(const StreamInfo& info) -> void = 0;

  /*
   * Called when a ChunkHeader message is received. Includes the data associated
   * with this chunk of stream data. This method should return the number of
   * bytes in this chunk that were actually used; leftover bytes will be
   * prepended to the next call.
   */
  virtual auto ProcessChunk(const cpp::span<std::byte>& chunk) -> void = 0;

  virtual auto ProcessEndOfStream() -> void = 0;

  virtual auto ProcessLogStatus() -> void {}

  /*
   * Called when there has been no data received over the input buffer for some
   * time. This could be used to synthesize output, or to save memory by
   * releasing unused resources.
   */
  virtual auto Process() -> void = 0;

 protected:
  auto SendOrBufferEvent(std::unique_ptr<StreamEvent> event) -> bool;

  // Queue for events coming into this element. Owned by us.
  QueueHandle_t input_events_;
  // Queue for events going into the next element. Not owned by us, may be null
  // if we're not yet in a pipeline.
  // FIXME: it would be nicer if this was non-nullable.
  QueueHandle_t output_events_;
  // Output events that have been generated, but are yet to be sent downstream.
  std::deque<std::unique_ptr<StreamEvent>> buffered_output_;
};

}  // namespace audio
