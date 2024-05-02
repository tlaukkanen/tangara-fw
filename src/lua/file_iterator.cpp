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
  current_()
 {
  auto lock = drivers::acquire_spi();

  const TCHAR* next_path = static_cast<const TCHAR*>(filepath.c_str());
  FRESULT res = f_opendir(&dir_, next_path);
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
  iterate(true);
}

auto FileIterator::iterate(bool reverse) -> bool {
    FILINFO info;
    if (reverse) {
      f_rewinddir(&dir_);
    }
    {
      auto lock = drivers::acquire_spi();
      auto res = f_readdir(&dir_, &info);
      if (res != FR_OK) {
        ESP_LOGI(kTag, "AAAAAAAAAAAAAAAAAAA");
        ESP_LOGI(kTag, "%d", res);
        return false;
      }
    }
    if (info.fname[0] == 0) {
      // End of directory
      current_.reset();
      ESP_LOGI(kTag, "End of dir");

    } else {
      // Update current value
      ESP_LOGI(kTag, "File: %s", info.fname);
      current_ = FileEntry{
        .isHidden = (info.fattrib & AM_HID) > 0,
        .isDirectory = (info.fattrib & AM_DIR) > 0,
        .isTrack = false, // TODO
        .filepath = original_path_ + info.fname,

      };
    }
    return true;
}

}  // namespace database