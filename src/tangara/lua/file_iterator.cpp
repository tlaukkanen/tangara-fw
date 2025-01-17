/*
 * Copyright 2023 ailurux <ailuruxx@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "lua/file_iterator.hpp"
#include "esp_log.h"

#include <string>

#include "drivers/spi.hpp"
#include "ff.h"

namespace lua {

[[maybe_unused]] static const char* kTag = "FileIterator";

FileIterator::FileIterator(std::string filepath, bool showHidden)
    : original_path_(filepath), show_hidden_(showHidden), current_(), offset_(-1) {
  const TCHAR* path = static_cast<const TCHAR*>(filepath.c_str());
  FRESULT res = f_opendir(&dir_, path);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "Error opening directory: %s", filepath.c_str());
  }
}

FileIterator::~FileIterator() {
  f_closedir(&dir_);
}

auto FileIterator::value() const -> const std::optional<FileEntry>& {
  return current_;
}

auto FileIterator::next() -> void {
  size_t prev_index = -1;
  if (current_) {
    prev_index = current_->index;
  }
  do {
    bool res = iterate(show_hidden_);
    if (!res) {
      break;
    }
  } while (!current_ || current_->index == prev_index);
}

auto FileIterator::prev() -> void {
  if (offset_ == 0) {
    current_.reset();
    return;
  }
  f_rewinddir(&dir_);
  auto new_offset = offset_ - 1;
  offset_ = -1;
  for (int i = 0; i <= new_offset; i++) {
    iterate(show_hidden_);
  }
}

auto FileIterator::iterate(bool show_hidden) -> bool {
  FILINFO info;
  auto res = f_readdir(&dir_, &info);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "Error reading directory. Error: %d", res);
    return false;
  }
  if (info.fname[0] == 0) {
    // End of directory
    // Set value to nil
    current_.reset();
    return false;
  } else {
    // Update current value
    offset_++;
    bool hidden =  (info.fattrib & AM_HID) > 0 || info.fname[0] == '.';
    if (!hidden || show_hidden) {
      current_ = FileEntry{
          .index = offset_,
          .isHidden = hidden,
          .isDirectory = (info.fattrib & AM_DIR) > 0,
          .filepath = original_path_ + (original_path_.size() > 0 ? "/" : "") +
                      info.fname,
          .name = info.fname,
      };
    }
  }
  return true;
}

}  // namespace lua
