#pragma once

#include <atomic>
#include <cstdint>

#include "freertos/FreeRTOS.h"

#include "chunk.hpp"
#include "freertos/message_buffer.h"
#include "freertos/portmacro.h"
#include "result.hpp"
#include "span.hpp"

#include "stream_buffer.hpp"
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
};

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
  IAudioElement()
      : input_buffer_(nullptr), output_buffer_(nullptr), state_(STATE_RUN) {}
  virtual ~IAudioElement() {}

  /*
   * Returns the stack size in bytes that this element requires. This should
   * be tuned according to the observed stack size of each element, as different
   * elements have fairly different stack requirements.
   */
  virtual auto StackSizeBytes() const -> std::size_t { return 2048; };

  /*
   * How long to wait for new data on the input stream before triggering a call
   * to ProcessIdle(). If this is portMAX_DELAY (the default), then ProcessIdle
   * will never be called.
   */
  virtual auto IdleTimeout() const -> TickType_t { return portMAX_DELAY; }

  virtual auto InputMinChunkSize() const -> std::size_t { return 0; }

  /* Returns this element's input buffer. */
  auto InputBuffer() const -> StreamBuffer* { return input_buffer_; }

  /* Returns this element's output buffer. */
  auto OutputBuffer() const -> StreamBuffer* { return output_buffer_; }

  auto InputBuffer(StreamBuffer* b) -> void { input_buffer_ = b; }

  auto OutputBuffer(StreamBuffer* b) -> void { output_buffer_ = b; }

  auto ElementState() const -> ElementState { return state_; }
  auto ElementState(enum ElementState e) -> void { state_ = e; }

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
  virtual auto ProcessIdle() -> cpp::result<void, AudioProcessingError> = 0;

  virtual auto PrepareForPause() -> void{};

 protected:
  StreamBuffer* input_buffer_;
  StreamBuffer* output_buffer_;
  std::atomic<enum ElementState> state_;
};

}  // namespace audio
