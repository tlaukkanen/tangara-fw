#include "fatfs_audio_input.hpp"

#include <cstdint>
#include <memory>
#include <string>

#include "esp_heap_caps.h"
#include "freertos/portmacro.h"

#include "audio_element.hpp"
#include "chunk.hpp"
#include "stream_buffer.hpp"
#include "stream_message.hpp"

static const char* kTag = "SRC";

namespace audio {

static const TickType_t kServiceInterval = pdMS_TO_TICKS(50);

static const std::size_t kFileBufferSize = 1024 * 128;
static const std::size_t kMinFileReadSize = 1024 * 4;

FatfsAudioInput::FatfsAudioInput(std::shared_ptr<drivers::SdStorage> storage)
    : IAudioElement(),
      storage_(storage),
      raw_file_buffer_(static_cast<std::byte*>(
          heap_caps_malloc(kFileBufferSize, MALLOC_CAP_SPIRAM))),
      file_buffer_(raw_file_buffer_, kFileBufferSize),
      file_buffer_read_pos_(file_buffer_.begin()),
      file_buffer_write_pos_(file_buffer_.begin()),
      current_file_(),
      is_file_open_(false),
      chunk_writer_(nullptr) {
  // TODO: create our chunk writer whenever the output buffer changes.
}

FatfsAudioInput::~FatfsAudioInput() {
  free(raw_file_buffer_);
}

auto FatfsAudioInput::ProcessStreamInfo(const StreamInfo& info)
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

  auto write_size =
      WriteMessage(TYPE_STREAM_INFO,
                   std::bind(&StreamInfo::Encode, info, std::placeholders::_1),
                   output_buffer_->WriteBuffer());

  if (write_size.has_error()) {
    return cpp::fail(IO_ERROR);
  } else {
    xMessageBufferSend(output_buffer_, output_buffer_->WriteBuffer().data(),
                       write_size.value(), portMAX_DELAY);
  }

  return {};
}

auto FatfsAudioInput::ProcessChunk(const cpp::span<std::byte>& chunk)
    -> cpp::result<size_t, AudioProcessingError> {
  return cpp::fail(UNSUPPORTED_STREAM);
}

auto FatfsAudioInput::GetRingBufferDistance() const -> size_t {
  if (file_buffer_read_pos_ == file_buffer_write_pos_) {
    return 0;
  }
  if (file_buffer_read_pos_ < file_buffer_write_pos_) {
    return file_buffer_write_pos_ - file_buffer_read_pos_;
  }
  return
      // Read position to end of buffer.
      (file_buffer_.end() - file_buffer_read_pos_)
      // Start of buffer to write position.
      + (file_buffer_write_pos_ - file_buffer_.begin());
}

auto FatfsAudioInput::ProcessIdle() -> cpp::result<void, AudioProcessingError> {
  // First, see if we're able to fill up the input buffer with any more of the
  // file's contents.
  if (is_file_open_) {
    size_t ringbuf_distance = GetRingBufferDistance();
    if (file_buffer_.size() - ringbuf_distance > kMinFileReadSize) {
      size_t read_size;
      if (file_buffer_write_pos_ < file_buffer_read_pos_) {
        // Don't worry about the start of buffer -> read pos size; we can get to
        // it next iteration.
        read_size = file_buffer_read_pos_ - file_buffer_write_pos_;
      } else {
        read_size = file_buffer_.begin() - file_buffer_write_pos_;
      }

      ESP_LOGI(kTag, "reading up to %d bytes", (int)read_size);

      UINT bytes_read = 0;
      FRESULT result =
          f_read(&current_file_, std::addressof(file_buffer_write_pos_),
                 read_size, &bytes_read);
      if (result != FR_OK) {
        ESP_LOGE(kTag, "file I/O error %d", result);
        return cpp::fail(IO_ERROR);
      }

      ESP_LOGI(kTag, "actual read size %d bytes", (int)bytes_read);

      if (f_eof(&current_file_)) {
        f_close(&current_file_);
        is_file_open_ = false;

        // TODO: open the next file?
      }

      file_buffer_write_pos_ += bytes_read;
      if (file_buffer_write_pos_ == file_buffer_.end()) {
        file_buffer_write_pos_ = file_buffer_.begin();
      }
    }
  } else if (GetRingBufferDistance() == 0) {
    // We have no file open, and no data waiting to be written. We're out of
    // stuff to do, so signal a pause.
    return cpp::fail(OUT_OF_DATA);
  }

  // Now stream data into the output buffer until it's full.
  while (GetRingBufferDistance() > 0) {
    ESP_LOGI(kTag, "writing up to %d bytes", (int)GetRingBufferDistance());
    ChunkWriteResult result = chunk_writer_->WriteChunkToStream(
        [&](cpp::span<std::byte> d) { return SendChunk(d); }, kServiceInterval);

    switch (result) {
      case CHUNK_WRITE_OKAY:
        break;
      case CHUNK_WRITE_TIMEOUT:
      case CHUNK_OUT_OF_DATA:
        // Both of these are fine; we will pick back up where we left off in
        // the next idle call.
        return {};
      default:
        return cpp::fail(IO_ERROR);
    }
  }

  // We've finished writing out chunks, but there may be more of the file to
  // read. Return, and begin again in the next idle call.
  return {};
}

auto FatfsAudioInput::SendChunk(cpp::span<std::byte> dest) -> size_t {
  if (file_buffer_read_pos_ == file_buffer_write_pos_) {
    return 0;
  }
  std::size_t chunk_size;
  if (file_buffer_read_pos_ > file_buffer_write_pos_) {
    chunk_size = file_buffer_.end() - file_buffer_read_pos_;
  } else {
    chunk_size = file_buffer_write_pos_ - file_buffer_read_pos_;
  }
  chunk_size = std::min(chunk_size, dest.size());

  cpp::span<std::byte> source(file_buffer_read_pos_, chunk_size);
  std::copy(source.begin(), source.end(), dest.begin());

  file_buffer_read_pos_ = file_buffer_read_pos_ + chunk_size;
  if (file_buffer_read_pos_ == file_buffer_.end()) {
    file_buffer_read_pos_ = file_buffer_.begin();
  }
  return chunk_size;
}

}  // namespace audio
