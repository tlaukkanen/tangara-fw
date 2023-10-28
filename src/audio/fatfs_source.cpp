/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "fatfs_source.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "esp_log.h"
#include "ff.h"

#include "audio_source.hpp"
#include "codec.hpp"
#include "spi.hpp"
#include "types.hpp"

namespace audio {

[[maybe_unused]] static constexpr char kTag[] = "fatfs_src";

FatfsSource::FatfsSource(codecs::StreamType t, std::unique_ptr<FIL> file)
    : IStream(t), file_(std::move(file)) {}

FatfsSource::~FatfsSource() {
  auto lock = drivers::acquire_spi();
  f_close(file_.get());
}

auto FatfsSource::Read(cpp::span<std::byte> dest) -> ssize_t {
  auto lock = drivers::acquire_spi();
  if (f_eof(file_.get())) {
    return 0;
  }
  UINT bytes_read = 0;
  FRESULT res = f_read(file_.get(), dest.data(), dest.size(), &bytes_read);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "error reading from file");
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
}  // namespace audio
