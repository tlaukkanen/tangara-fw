#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "freertos/FreeRTOS.h"

#include "cbor.h"
#include "freertos/message_buffer.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "result.hpp"
#include "span.hpp"
#include "stream_buffer.hpp"

namespace audio {

/**
 * Utility for handling an input stream of chunk data, which simplifies needing
 * access to blocks of data spanning two chunks.
 */
class ChunkReader {
 public:
  explicit ChunkReader(std::size_t chunk_size);
  ~ChunkReader();

  auto HandleBytesLeftOver(std::size_t bytes_left) -> void;
  auto HandleBytesUsed(std::size_t bytes_used) -> void;

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
  auto HandleNewData(cpp::span<std::byte> data) -> cpp::span<std::byte>;

  auto GetLeftovers() -> cpp::span<std::byte>;

  ChunkReader(const ChunkReader&) = delete;
  ChunkReader& operator=(const ChunkReader&) = delete;

 private:
  std::byte* raw_working_buffer_;
  cpp::span<std::byte> working_buffer_;
  cpp::span<std::byte> last_data_in_working_buffer_;
  std::size_t leftover_bytes_ = 0;
};

}  // namespace audio
