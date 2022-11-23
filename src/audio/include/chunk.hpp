#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include "esp-idf/components/cbor/tinycbor/src/cbor.h"
#include "freertos/portmacro.h"
#include "result.hpp"

namespace audio {

extern const std::size_t kMaxChunkSize;

enum ChunkWriteResult {
  // Returned when the callback does not write any data.
  CHUNK_OUT_OF_DATA,
  // Returned when there is an error encoding a chunk header using cbor.
  CHUNK_ENCODING_ERROR,
  // Returned when max_wait expires without room in the stream buffer becoming
  // available.
  CHUNK_WRITE_TIMEOUT,
};

/*
 * Invokes the given callback to receive data, breaks the received data up into
 * chunks with headers, and writes those chunks to the given output stream.
 *
 * The callback will be invoked with a byte buffer and its size. The callback
 * should write as much data as it can to this buffer, and then return the
 * number of bytes it wrote. Return a value of 0 to indicate that there is no
 * more input to read.
 */
auto WriteChunksToStream(MessageBufferHandle_t* stream,
                         uint8_t* working_buffer,
                         size_t working_buffer_length,
                         std::function<size_t(uint8_t*, size_t)> callback,
                         TickType_t max_wait) -> EncodeWriteResult;

class ChunkReader {
 public:
  ChunkReader(MessageBufferHandle_t* stream);
  ~ChunkReader();

  auto Reset() -> void;

  auto GetLastMessage() -> std::pair<uint8_t*, size_t>;

  enum ChunkReadResult {
    // Returned an error in parsing the cbor-encoded header.
    CHUNK_DECODING_ERROR,
    // Returned when max_wait expired before any data was read.
    CHUNK_READ_TIMEOUT,
    // Returned when a non-chunk message is received.
    CHUNK_STREAM_ENDED,
    // Returned when the processing callback does not return a value.
    CHUNK_PROCESSING_ERROR,
  };

  /*
   * Reads chunks of data from the given input stream, and invokes the given
   * callback to process each of them in turn.
   *
   * The callback will be invoked with a byte buffer and its size. The callback
   * should process as much data as it can from this buffer, and then return the
   * number of bytes it was able to read. Any leftover bytes will be added as a
   * prefix to the next chunk.
   *
   * If this function encounters a message in the stream that is not a chunk, it
   * will place the message at the start of the working_buffer and then return.
   */
  auto ReadChunkFromStream(
      std::function<std::optional<size_t>(uint8_t*, size_t)> callback,
      TickType_t max_wait) -> EncodeReadResult;

 private:
  MessageBufferHandle_t* stream_;
  uint8_t* working_buffer_;

  std::size_t leftover_bytes_ = 0;
  std::size_t last_message_size_ = 0;
};

}  // namespace audio
