#include "fatfs_audio_input.hpp"

#include <cstdint>
#include <memory>
#include <string>

#include "audio_element.hpp"
#include "esp_heap_caps.h"
#include "freertos/portmacro.h"

#include "audio_element.hpp"
#include "chunk.hpp"

namespace audio {

static const TickType_t kServiceInterval = pdMS_TO_TICKS(50);

static const std::size_t kFileBufferSize = 1024 * 128;
static const std::size_t kMinFileReadSize = 1024 * 4;
static const std::size_t kOutputBufferSize = 1024 * 4;

FatfsAudioInput::FatfsAudioInput(std::shared_ptr<drivers::SdStorage> storage)
    : IAudioElement(), storage_(storage) {
  file_buffer_ = static_cast<uint8_t*>(
      heap_caps_malloc(kFileBufferSize, MALLOC_CAP_SPIRAM));
  file_buffer_read_pos_ = file_buffer_;
  file_buffer_write_pos_ = file_buffer_;
  chunk_buffer_ =
      static_cast<uint8_t*>(heap_caps_malloc(kMaxChunkSize, MALLOC_CAP_SPIRAM));

  output_buffer_memory_ = static_cast<uint8_t*>(
      heap_caps_malloc(kOutputBufferSize, MALLOC_CAP_SPIRAM));
  output_buffer_ = new MessageBufferHandle_t;
  *output_buffer_ = xMessageBufferCreateStatic(
      kOutputBufferSize, output_buffer_memory_, &output_buffer_metadata_);
}

FatfsAudioInput::~FatfsAudioInput() {
  free(file_buffer_);
  free(chunk_buffer_);
  vMessageBufferDelete(output_buffer_);
  free(output_buffer_memory_);
  free(output_buffer_);
}

auto FatfsAudioInput::ProcessStreamInfo(StreamInfo&& info)
    -> cpp::result<void, StreamError> {
  if (is_file_open_) {
    f_close(&current_file_);
    is_file_open_ = false;
  }

  std::string path = info.Path().value_or("");
  FRESULT res = f_open(&current_file_, path.c_str(), FA_READ);
  if (res != FR_OK) {
    return cpp::fail(IO_ERROR);
  }

  is_file_open_ = true;

  // TODO: pass on stream info.

  return {};
}

auto FatfsAudioInput::ProcessChunk(uint8_t* data, std::size_t length)
    -> cpp::result<size_t, StreamError> {
  // TODO.
  return 0;
}

auto FatfsAudioInput::GetRingBufferDistance() -> size_t {
  if (file_buffer_read_pos_ == file_buffer_write_pos_) {
    return 0;
  }
  if (file_buffer_read_pos_ < file_buffer_write_pos_) {
    return file_buffer_write_pos_ - file_buffer_read_pos_;
  }
  return
      // Read position to end of buffer.
      (file_buffer_ + kFileBufferSize - file_buffer_read_pos_)
      // Start of buffer to write position.
      + (file_buffer_write_pos_ - file_buffer_);
}

auto FatfsAudioInput::ProcessIdle() -> cpp::result<void, StreamError> {
  // First, see if we're able to fill up the input buffer with any more of the
  // file's contents.
  if (is_file_open_) {
    size_t ringbuf_distance = GetRingBufferDistance();
    if (kFileBufferSize - ringbuf_distance > kMinFileReadSize) {
      size_t read_size;
      if (file_buffer_write_pos_ < file_buffer_read_pos_) {
        // Don't worry about the start of buffer -> read pos size; we can get to
        // it next iteration.
        read_size = file_buffer_read_pos_ - file_buffer_write_pos_;
      } else {
        read_size = file_buffer_ - file_buffer_write_pos_;
      }

      UINT bytes_read = 0;
      FRESULT result = f_read(&current_file_, file_buffer_write_pos_, read_size,
                              &bytes_read);
      if (result != FR_OK) {
        return cpp::fail(IO_ERROR);  // TODO;
      }

      if (f_eof(&current_file_)) {
        f_close(&current_file_);
        is_file_open_ = false;

        // TODO: open the next file?
      }

      file_buffer_write_pos_ += bytes_read;
      if (file_buffer_write_pos_ == file_buffer_ + kFileBufferSize) {
        file_buffer_write_pos_ = file_buffer_;
      }
    }
  }

  // Now stream data into the output buffer until it's full.
  pending_read_pos_ = nullptr;
  ChunkWriteResult result = WriteChunksToStream(
      output_buffer_, chunk_buffer_, kMaxChunkSize,
      [&](uint8_t* b, size_t s) { return SendChunk(b, s); }, kServiceInterval);

  switch (result) {
    case CHUNK_WRITE_TIMEOUT:
    case CHUNK_OUT_OF_DATA:
      return {};  // TODO.
    default:
      return {};  // TODO.
  }
}

auto FatfsAudioInput::SendChunk(uint8_t* buffer, size_t size) -> size_t {
  if (pending_read_pos_ != nullptr) {
    file_buffer_read_pos_ = pending_read_pos_;
  }

  if (file_buffer_read_pos_ == file_buffer_write_pos_) {
    return 0;
  }
  std::size_t write_size;
  if (file_buffer_read_pos_ > file_buffer_write_pos_) {
    write_size = file_buffer_ + kFileBufferSize - file_buffer_read_pos_;
  } else {
    write_size = file_buffer_write_pos_ - file_buffer_read_pos_;
  }
  write_size = std::min(write_size, size);
  memcpy(buffer, file_buffer_read_pos_, write_size);

  pending_read_pos_ = file_buffer_read_pos_ + write_size;
  if (pending_read_pos_ == file_buffer_ + kFileBufferSize) {
    pending_read_pos_ = file_buffer_;
  }
  return write_size;
}

}  // namespace audio
