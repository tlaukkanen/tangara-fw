/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "readahead_source.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "ff.h"

#include "audio_source.hpp"
#include "codec.hpp"
#include "freertos/portmacro.h"
#include "spi.hpp"
#include "tasks.hpp"
#include "types.hpp"

namespace audio {

static constexpr char kTag[] = "readahead";
static constexpr size_t kBufferSize = 1024 * 512;

ReadaheadSource::ReadaheadSource(tasks::WorkerPool& worker,
                                 std::unique_ptr<codecs::IStream> wrapped)
    : IStream(wrapped->type()),
      worker_(worker),
      wrapped_(std::move(wrapped)),
      readahead_enabled_(false),
      is_refilling_(false),
      buffer_(xStreamBufferCreateWithCaps(kBufferSize, 1, MALLOC_CAP_SPIRAM)),
      tell_(wrapped_->CurrentPosition()) {}

ReadaheadSource::~ReadaheadSource() {
  is_refilling_.wait(true);
  vStreamBufferDeleteWithCaps(buffer_);
}

auto ReadaheadSource::Read(std::span<std::byte> dest) -> ssize_t {
  size_t bytes_written = 0;
  // Fill the destination from our buffer, until either the buffer is drained
  // or the destination is full.
  while (!dest.empty() && (is_refilling_ || !xStreamBufferIsEmpty(buffer_))) {
    size_t bytes_read =
        xStreamBufferReceive(buffer_, dest.data(), dest.size_bytes(), 1);
    tell_ += bytes_read;
    bytes_written += bytes_read;
    dest = dest.subspan(bytes_read);
  }

  // After the loop, we've either written everything that was asked for, or
  // we're out of data.
  if (!dest.empty()) {
    // Out of data in the buffer. Finish using the wrapped stream.
    size_t extra_bytes = wrapped_->Read(dest);
    tell_ += extra_bytes;
    bytes_written += extra_bytes;

    // Check for EOF in the wrapped stream.
    if (extra_bytes < dest.size_bytes()) {
      return bytes_written;
    }
  }
  // After this point, we're done writing to `dest`. It's either empty, or the
  // underlying source is EOF.

  // If we're here, then there is more data to be read from the wrapped stream.
  // Ensure the readahead is running.
  if (!is_refilling_ && readahead_enabled_ &&
      xStreamBufferBytesAvailable(buffer_) < kBufferSize / 4) {
    BeginReadahead();
  }

  return bytes_written;
}

auto ReadaheadSource::CanSeek() -> bool {
  return wrapped_->CanSeek();
}

auto ReadaheadSource::SeekTo(int64_t destination, SeekFrom from) -> void {
  // Seeking blows away all of our prefetched data. To do this safely, we
  // first need to wait for the refill task to finish.
  ESP_LOGI(kTag, "dropping readahead due to seek");
  is_refilling_.wait(true);
  // It's now safe to clear out the buffer.
  xStreamBufferReset(buffer_);

  wrapped_->SeekTo(destination, from);

  // Make sure our tell is up to date with the new location.
  tell_ = wrapped_->CurrentPosition();
}

auto ReadaheadSource::CurrentPosition() -> int64_t {
  return tell_;
}

auto ReadaheadSource::Size() -> std::optional<int64_t> {
  return wrapped_->Size();
}

auto ReadaheadSource::SetPreambleFinished() -> void {
  readahead_enabled_ = true;
  BeginReadahead();
}

auto ReadaheadSource::BeginReadahead() -> void {
  is_refilling_ = true;
  std::function<void(void)> refill = [this]() {
    // Try to keep larger than most reasonable FAT sector sizes for more
    // efficient disk reads.
    constexpr size_t kMaxSingleRead = 1024 * 16;
    std::byte working_buf[kMaxSingleRead];
    for (;;) {
      size_t bytes_to_read = std::min<size_t>(
          kMaxSingleRead, xStreamBufferSpacesAvailable(buffer_));
      if (bytes_to_read == 0) {
        break;
      }
      size_t read = wrapped_->Read({working_buf, bytes_to_read});
      if (read > 0) {
        xStreamBufferSend(buffer_, working_buf, read, 0);
      }
      if (read < bytes_to_read) {
        break;
      }
    }
    is_refilling_ = false;
    is_refilling_.notify_all();
  };
  worker_.Dispatch(refill);
}

}  // namespace audio
