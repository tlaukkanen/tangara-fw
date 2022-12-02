#include "fatfs_audio_input.hpp"

#include <cstdint>
#include <memory>
#include <string>

#include "audio_element.hpp"
#include "esp_heap_caps.h"
#include "freertos/portmacro.h"

#include "audio_element.hpp"
#include "chunk.hpp"
#include "stream_message.hpp"

static const char* kTag = "SRC";

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

auto FatfsAudioInput::ProcessStreamInfo(StreamInfo& info)
    -> cpp::result<void, AudioProcessingError> {
  if (is_file_open_) {
    f_close(&current_file_);
    is_file_open_ = false;
  }

  if (!info.Path()) {
    return cpp::fail(UNSUPPORTED_STREAM);
  }
  std::string path = info.Path().value();
  FRESULT res = f_open(&current_file_, path.c_str(), FA_READ);
  if (res != FR_OK) {
    return cpp::fail(IO_ERROR);
  }

  is_file_open_ = true;

  auto write_res =
      WriteMessage(TYPE_STREAM_INFO,
                   std::bind(&StreamInfo::Encode, info, std::placeholders::_1),
                   chunk_buffer_, kMaxChunkSize);

  if (write_res.has_error()) {
    return cpp::fail(IO_ERROR);
  } else {
    xMessageBufferSend(output_buffer_, chunk_buffer_, write_res.value(),
                       portMAX_DELAY);
  }

  return {};
}

auto FatfsAudioInput::ProcessChunk(uint8_t* data, std::size_t length)
    -> cpp::result<size_t, AudioProcessingError> {
  return cpp::fail(UNSUPPORTED_STREAM);
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

auto FatfsAudioInput::ProcessIdle() -> cpp::result<void, AudioProcessingError> {
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
        ESP_LOGE(kTag, "file I/O error %d", result);
        return cpp::fail(IO_ERROR);
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
      // Both of these are fine; SendChunk keeps track of where it's up to
      // internally, so we will pick back up where we left off.
      return {};
    default:
      return cpp::fail(IO_ERROR);
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
