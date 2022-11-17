#pragma once

#include <cstdint>

namespace audio {

extern const std::size_t kMaxFrameSize;

class IAudioElement {
 public:
  virtual ~IAudioElement();

  enum CommandType {
    /*
     * Sets the sequence number of the most recent byte stream. Any commands
     * received that have a lower sequence number than this will be discarded.
     */
    SEQUENCE_NUMBER,
    /*
     * Instructs this element to read a specific number of bytes from its
     * input buffer.
     */
    READ_FRAME,
    /*
     * Represents an element-specific command. This handling of this is
     * delegated to element implementations.
     */
    ELEMENT,
    /* Instructs this element to shut down. */
    QUIT,
  };

  struct Command {
    CommandType type;
    uint8_t sequence_number;
    // TODO: tag data's type
    union {
      void* data;
      std::size_t frame_size;
    };
  };

  /*
   * Returns a queue that should be used for all communication with this
   * element.
   */
  virtual auto InputCommandQueue() -> QueueHandle_t = 0;

  /*
   * Returns a buffer that will be used to stream input bytes to this element.
   * This may be NULL, if this element represents a source, e.g. a FATFS
   * reader.
   */
  virtual auto InputBuffer() -> StreamBufferHandle_t = 0;

  enum ProcessResult {
    OK,
    OUTPUT_FULL,
    ERROR,
  };

  /*
   * Called when an element-specific command has been received.
   */
  virtual auto ProcessElementCommand(void* command) -> ProcessResult = 0;

  virtual auto SkipElementCommand(void* command) -> void = 0;

  /*
   * Called with the result of a read bytes command.
   */
  virtual auto ProcessData(uint8_t* data, uint16_t length) -> ProcessResult = 0;

  /*
   * Called periodically when there are no pending commands.
   */
  virtual auto ProcessIdle() -> ProcessResult = 0;
};

}  // namespace audio
