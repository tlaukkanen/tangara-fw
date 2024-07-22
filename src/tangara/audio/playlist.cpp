/*
 * Copyright 2024 ailurux <ailuruxx@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "playlist.hpp"

#include <string.h>

#include "audio/playlist.hpp"
#include "database/database.hpp"
#include "esp_log.h"
#include "ff.h"

namespace audio {
[[maybe_unused]] static constexpr char kTag[] = "playlist";

Playlist::Playlist(std::string playlistFilepath)
    : filepath_(playlistFilepath),
      mutex_(),
      total_size_(0),
      pos_(-1),
      offset_cache_(&memory::kSpiRamResource),
      sample_size_(50) {}

auto Playlist::open() -> bool {
  FRESULT res =
      f_open(&file_, filepath_.c_str(), FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "failed to open file! res: %i", res);
    return false;
  }
  // Count all entries
  consumeAndCount(-1);
  // Grab the first one
  skipTo(0);
  return true;
}

Playlist::~Playlist() {
  f_close(&file_);
}

auto Playlist::currentPosition() const -> size_t {
  return pos_;
}

auto Playlist::size() const -> size_t {
  return total_size_;
}

auto Playlist::append(Item i) -> void {
  std::unique_lock<std::mutex> lock(mutex_);
  auto offset = f_tell(&file_);
  bool first_entry = current_value_.empty();
  // Seek to end and append
  auto end = f_size(&file_);
  auto res = f_lseek(&file_, end);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "Seek to end of file failed? Error %d", res);
    return;
  }
  // TODO: Resolve paths for track id, etc
  std::string path;
  if (std::holds_alternative<std::string>(i)) {
    path = std::get<std::string>(i);
    f_printf(&file_, "%s\n", path.c_str());
    if (total_size_ % sample_size_ == 0) {
      offset_cache_.push_back(end);
    }
    if (first_entry) {
      current_value_ = path;
    }
    total_size_++;
  }
  // Restore position
  res = f_lseek(&file_, offset);
    if (res != FR_OK) {
      ESP_LOGE(kTag, "Failed to restore file position after append?");
      return;
  }
  res = f_sync(&file_);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "Failed to sync playlist file after append");
    return;
  }
}

auto Playlist::skipTo(size_t position) -> void {
  std::unique_lock<std::mutex> lock(mutex_);
  // Check our cache and go to nearest entry
  pos_ = position;
  auto remainder = position % sample_size_;
  auto quotient = (position - remainder) / sample_size_;
  if (offset_cache_.size() < quotient) {
    // Fall back case
    ESP_LOGW(kTag, "File offset cache failed, falling back...");
    f_rewind(&file_);
    advanceBy(pos_);
    return;
  }
  auto entry = offset_cache_.at(quotient);
  // Go to byte offset
  auto res = f_lseek(&file_, entry);
  if (res != FR_OK) {
    ESP_LOGW(kTag, "Error going to byte offset %llu for playlist entry index %d", entry, pos_);
  }
  // Count ahead entries
  advanceBy(remainder+1);
}

auto Playlist::next() -> void {
  if (!atEnd()) {
    pos_++;
    skipTo(pos_);
  }
}

auto Playlist::prev() -> void {
  // Naive approach to see how that goes for now
  pos_--;
  skipTo(pos_);
}

auto Playlist::value() const -> std::string {
  return current_value_;
}

auto Playlist::clear() -> bool {
  std::unique_lock<std::mutex> lock(mutex_);
  auto res = f_close(&file_);
  if (res != FR_OK) {
    return false;
  }
  res =
      f_open(&file_, filepath_.c_str(), FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
  if (res != FR_OK) {
    return false;
  }
  total_size_ = 0;
  current_value_.clear();
  offset_cache_.clear();
  pos_ = 0;
  return true;
}

auto Playlist::atEnd() -> bool {
  return pos_ + 1 >= total_size_;
}

auto Playlist::filepath() -> std::string {
  return filepath_;
}

auto Playlist::consumeAndCount(ssize_t upto) -> bool {
  std::unique_lock<std::mutex> lock(mutex_);
  TCHAR buff[512];
  size_t count = 0;
  f_rewind(&file_);
  while (!f_eof(&file_)) {
    auto offset = f_tell(&file_);
    // TODO: Correctly handle lines longer than this
    // TODO: Also correctly handle the case where the last entry doesn't end in
    // \n
    auto res = f_gets(buff, 512, &file_);
    if (res == NULL) {
      ESP_LOGW(kTag, "Error consuming playlist file at line %d", count);
      return false;
    }
    if (count % sample_size_ == 0) {
      offset_cache_.push_back(offset);
    }
    count++;

    if (upto >= 0 && count > upto) {
      size_t len = strlen(buff);
      current_value_.assign(buff, len - 1);
      break;
    }
  }
  if (upto < 0) {
    total_size_ = count;
    f_rewind(&file_);
  }
  return true;
}

auto Playlist::advanceBy(ssize_t amt) -> bool {
  TCHAR buff[512];
  size_t count = 0;
  while (!f_eof(&file_)) {
    auto res = f_gets(buff, 512, &file_);
    if (res == NULL) {
      ESP_LOGW(kTag, "Error consuming playlist file at line %d", count);
      return false;
    }
    count++;
    if (count >= amt) {
      size_t len = strlen(buff);
      current_value_.assign(buff, len - 1);
      break;
    }
  }
  return true;
}

}  // namespace audio