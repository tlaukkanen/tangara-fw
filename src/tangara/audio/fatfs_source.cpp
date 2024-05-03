/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "audio/fatfs_source.hpp"
#include <sys/_stdint.h>

#include <cstddef>
#include <cstdint>
#include <memory>

#include "esp_log.h"
#include "events/event_queue.hpp"
#include "ff.h"

#include "audio/audio_source.hpp"
#include "codec.hpp"
#include "drivers/spi.hpp"
#include "system_fsm/system_events.hpp"
#include "types.hpp"

namespace audio {

[[maybe_unused]] static constexpr char kTag[] = "fatfs_src";

FatfsSource::FatfsSource(codecs::StreamType t, std::unique_ptr<FIL> file)
    : IStream(t), file_(std::move(file)) {}

FatfsSource::~FatfsSource() {
  auto lock = drivers::acquire_spi();
  f_close(file_.get());
}

auto FatfsSource::Read(std::span<std::byte> dest) -> ssize_t {
  auto lock = drivers::acquire_spi();
  if (f_eof(file_.get())) {
    return 0;
  }
  UINT bytes_read = 0;
  FRESULT res = f_read(file_.get(), dest.data(), dest.size(), &bytes_read);
  if (res != FR_OK) {
    events::System().Dispatch(system_fsm::StorageError{.error = res});
    return -1;
  }
  return bytes_read;
}

auto FatfsSource::CanSeek() -> bool {
  return true;
}

auto FatfsSource::SeekTo(int64_t destination, SeekFrom from) -> void {
  auto lock = drivers::acquire_spi();
  switch (from) {
    case SeekFrom::kStartOfStream:
      f_lseek(file_.get(), destination);
      break;
    case SeekFrom::kEndOfStream:
      f_lseek(file_.get(), f_size(file_.get()) + destination);
      break;
    case SeekFrom::kCurrentPosition:
      f_lseek(file_.get(), f_tell(file_.get()) + destination);
      break;
  }
}

auto FatfsSource::CurrentPosition() -> int64_t {
  return f_tell(file_.get());
}

auto FatfsSource::Size() -> std::optional<int64_t> {
  return f_size(file_.get());
}

}  // namespace audio
