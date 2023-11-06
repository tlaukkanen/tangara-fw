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
#include "idf_additions.h"
#include "spi.hpp"
#include "tasks.hpp"
#include "types.hpp"

namespace audio {

static constexpr char kTag[] = "readahead";
static constexpr size_t kBufferSize = 1024 * 512;

ReadaheadSource::ReadaheadSource(tasks::Worker& worker,
                                 std::unique_ptr<codecs::IStream> wrapped)
    : IStream(wrapped->type()),
      worker_(worker),
      wrapped_(std::move(wrapped)),
      is_refilling_(false),
      buffer_(xStreamBufferCreateWithCaps(kBufferSize, 1, MALLOC_CAP_SPIRAM)),
      tell_(wrapped_->CurrentPosition()) {}

ReadaheadSource::~ReadaheadSource() {
  vStreamBufferDeleteWithCaps(buffer_);
}

auto ReadaheadSource::Read(cpp::span<std::byte> dest) -> ssize_t {
  // Optismise for the most frequent case: the buffer already contains enough
  // data for this call.
  size_t bytes_read =
      xStreamBufferReceive(buffer_, dest.data(), dest.size_bytes(), 0);

  tell_ += bytes_read;
  if (bytes_read == dest.size_bytes()) {
    return bytes_read;
  }

  dest = dest.subspan(bytes_read);

  // Are we currently fetching more bytes?
  ssize_t extra_bytes = 0;
  if (!is_refilling_) {
    // No! Pass through directly to the wrapped source for the fastest
    // response.
    extra_bytes = wrapped_->Read(dest);
  } else {
    // Yes! Wait for the refill to catch up, then try again.
    is_refilling_.wait(true);
    extra_bytes =
        xStreamBufferReceive(buffer_, dest.data(), dest.size_bytes(), 0);
  }

  // No need to check whether the dest buffer is actually filled, since at this
  // point we've read as many bytes as were available.
  tell_ += extra_bytes;
  bytes_read += extra_bytes;

  // Before returning, make sure the readahead task is kicked off again.
  ESP_LOGI(kTag, "triggering readahead");
  is_refilling_ = true;
  std::function<void(void)> refill = [this]() {
    // Try to keep larger than most reasonable FAT sector sizes for more
    // efficient disk reads.
    constexpr size_t kMaxSingleRead = 1024 * 64;
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

  return bytes_read;
}

auto ReadaheadSource::CanSeek() -> bool {
  return wrapped_->CanSeek();
}

auto ReadaheadSource::SeekTo(int64_t destination, SeekFrom from) -> void {
  // Seeking blows away all of our prefetched data. To do this safely, we
  // first need to wait for the refill task to finish.
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
}  // namespace audio
