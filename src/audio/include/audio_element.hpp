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

enum ElementState {
  STATE_RUN,
  STATE_PAUSE,
  STATE_QUIT,
};

/*
 * Errors that may be returned by any of the Process* methods of an audio
 * element.
 */
enum AudioProcessingError {
  // Indicates that this element is unable to handle the upcoming chunks.
  UNSUPPORTED_STREAM,
  // Indicates an error with reading or writing stream data.
  IO_ERROR,
  // Indicates that the element has run out of data to process.
  OUT_OF_DATA,
};

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
  virtual auto StackSizeBytes() const -> std::size_t { return 2048; };

  virtual auto InputMinChunkSize() const -> std::size_t { return 0; }

  /* Returns this element's input buffer. */
  auto InputEventQueue() const -> QueueHandle_t { return input_events_; }

  /* Returns this element's output buffer. */
  auto OutputEventQueue() const -> QueueHandle_t { return output_events_; }

  auto OutputEventQueue(const QueueHandle_t q) -> void { output_events_ = q; }

  auto HasUnflushedOutput() -> bool { return !buffered_output_.empty(); }

  virtual auto HasUnprocessedInput() -> bool = 0;

  auto IsOverBuffered() -> bool { return unprocessed_output_chunks_ > 4; }

  auto FlushBufferedOutput() -> bool;

  auto ElementState() const -> ElementState { return current_state_; }
  auto ElementState(enum ElementState e) -> void { current_state_ = e; }

  virtual auto OnChunkProcessed() -> void { unprocessed_output_chunks_--; }

  /*
   * Called when a StreamInfo message is received. Used to configure this
   * element in preperation for incoming chunks.
   */
  virtual auto ProcessStreamInfo(const StreamInfo& info)
      -> cpp::result<void, AudioProcessingError> = 0;

  /*
   * Called when a ChunkHeader message is received. Includes the data associated
   * with this chunk of stream data. This method should return the number of
   * bytes in this chunk that were actually used; leftover bytes will be
   * prepended to the next call.
   */
  virtual auto ProcessChunk(const cpp::span<std::byte>& chunk)
      -> cpp::result<std::size_t, AudioProcessingError> = 0;

  /*
   * Called when there has been no data received over the input buffer for some
   * time. This could be used to synthesize output, or to save memory by
   * releasing unused resources.
   */
  virtual auto Process() -> cpp::result<void, AudioProcessingError> = 0;

 protected:
  auto SendOrBufferEvent(std::unique_ptr<StreamEvent> event) -> bool;

  // Queue for events coming into this element. Owned by us.
  QueueHandle_t input_events_;
  // Queue for events going into the next element. Not owned by us, may be null
  // if we're not yet in a pipeline.
  // FIXME: it would be nicer if this was non-nullable.
  QueueHandle_t output_events_;

  // The number of output chunks that we have generated, but have not yet been
  // processed by the next element in the pipeline. This includes any chunks
  // that are currently help in buffered_output_.
  int unprocessed_output_chunks_;
  // Output events that have been generated, but are yet to be sent downstream.
  std::deque<std::unique_ptr<StreamEvent>> buffered_output_;

  enum ElementState current_state_;
};

}  // namespace audio
