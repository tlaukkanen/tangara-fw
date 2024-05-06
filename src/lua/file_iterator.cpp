/*
 * Copyright 2023 ailurux <ailuruxx@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "file_iterator.hpp"
#include "esp_log.h"

#include <string>

#include "ff.h"
#include "spi.hpp"

namespace database {

[[maybe_unused]] static const char* kTag = "FileIterator";

FileIterator::FileIterator(std::string filepath) 
: original_path_(filepath),
  current_(),
  offset_(-1)
 {
  auto lock = drivers::acquire_spi();

  const TCHAR* path = static_cast<const TCHAR*>(filepath.c_str());
  FRESULT res = f_opendir(&dir_, path);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "Error opening directory: %s", filepath.c_str());
  }
}

FileIterator::~FileIterator() {
  auto lock = drivers::acquire_spi();
  f_closedir(&dir_);
}

auto FileIterator::value() const -> const std::optional<FileEntry>& {
  return current_;
}

auto FileIterator::next() -> void {
  iterate(false);
}

auto FileIterator::prev() -> void {
  if (offset_ == 0) {
    current_.reset();
    return;
  }
  f_rewinddir(&dir_);
  auto new_offset = offset_-1;
  offset_ = -1;
  for (int i = 0; i <= new_offset; i++) {
    iterate(false);
  }
}

auto FileIterator::iterate(bool reverse) -> bool {
    FILINFO info;
    {
      auto lock = drivers::acquire_spi();
      auto res = f_readdir(&dir_, &info);
      if (res != FR_OK) {
        ESP_LOGE(kTag, "Error reading directory. Error: %d", res);
        return false;
      }
    }
    if (info.fname[0] == 0) {
      // End of directory
      // Set value to nil
      current_.reset();
    } else {
      // Update current value
      offset_++;
      current_ = FileEntry{
        .index = offset_,
        .isHidden = (info.fattrib & AM_HID) > 0,
        .isDirectory = (info.fattrib & AM_DIR) > 0,
        .isTrack = false, // TODO
        .filepath = original_path_ + (original_path_.size()>0?"/":"") + info.fname,

      };
    }
    return true;
}

}  // namespace database