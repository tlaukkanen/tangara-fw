/*
 * Copyright 2024 ailurux <ailuruxx@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "playlist.hpp"

#include <string>

#include "esp_log.h"
#include "ff.h"

#include "audio/playlist.hpp"
#include "database/database.hpp"

namespace audio {

[[maybe_unused]] static constexpr char kTag[] = "playlist";

Playlist::Playlist(const std::string& playlistFilepath)
    : filepath_(playlistFilepath),
      mutex_(),
      total_size_(0),
      pos_(-1),
      file_open_(false),
      file_error_(false),
      offset_cache_(&memory::kSpiRamResource),
      sample_size_(50) {}

auto Playlist::open() -> bool {
  std::unique_lock<std::mutex> lock(mutex_);
  if (file_open_) {
    return true;
  }

  FRESULT res =
      f_open(&file_, filepath_.c_str(), FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "failed to open file! res: %i", res);
    return false;
  }
  file_open_ = true;
  file_error_ = false;

  // Count the playlist size and build our offset cache.
  countItems();
  // Advance to the first item.
  skipToWithoutCache(0);

  return !file_error_;
}

Playlist::~Playlist() {
  if (file_open_) {
    f_close(&file_);
  }
}

auto Playlist::filepath() const -> std::string {
  return filepath_;
}

auto Playlist::currentPosition() const -> size_t {
  std::unique_lock<std::mutex> lock(mutex_);
  return pos_ < 0 ? 0 : pos_;
}

auto Playlist::size() const -> size_t {
  std::unique_lock<std::mutex> lock(mutex_);
  return total_size_;
}

auto Playlist::value() const -> std::string {
  std::unique_lock<std::mutex> lock(mutex_);
  return current_value_;
}

auto Playlist::atEnd() const -> bool {
  std::unique_lock<std::mutex> lock(mutex_);
  return pos_ + 1 >= total_size_;
}

auto Playlist::next() -> void {
  std::unique_lock<std::mutex> lock(mutex_);
  if (pos_ + 1 < total_size_ && !file_error_) {
    advanceBy(1);
  }
}

auto Playlist::prev() -> void {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!file_error_) {
    // Naive approach to see how that goes for now
    skipToLocked(pos_ - 1);
  }
}

auto Playlist::skipTo(size_t position) -> void {
  std::unique_lock<std::mutex> lock(mutex_);
  skipToLocked(position);
}

auto Playlist::skipToLocked(size_t position) -> void {
  if (!file_open_ || file_error_) {
    return;
  }

  // Check our cache and go to nearest entry
  auto remainder = position % sample_size_;
  auto quotient = (position - remainder) / sample_size_;
  if (offset_cache_.size() <= quotient) {
    skipToWithoutCache(position);
    return;
  }

  // Go to byte offset
  auto entry = offset_cache_.at(quotient);
  auto res = f_lseek(&file_, entry);
  if (res != FR_OK) {
    ESP_LOGW(kTag, "error seeking %u", res);
    file_error_ = true;
    return;
  }

  // Count ahead entries.
  advanceBy(remainder + 1);
}

auto Playlist::skipToWithoutCache(size_t position) -> void {
  if (position >= pos_) {
    advanceBy(position - pos_);
  } else {
    pos_ = -1;
    FRESULT res = f_rewind(&file_);
    if (res != FR_OK) {
      ESP_LOGW(kTag, "error rewinding %u", res);
      file_error_ = true;
      return;
    }
    advanceBy(position + 1);
  }
}

auto Playlist::countItems() -> void {
  TCHAR buff[512];

  for (;;) {
    auto offset = f_tell(&file_);
    auto next_item = nextItem(buff);
    if (!next_item) {
      break;
    }
    if (total_size_ % sample_size_ == 0) {
      offset_cache_.push_back(offset);
    }
    total_size_++;
  }

  f_rewind(&file_);
}

auto Playlist::advanceBy(ssize_t amt) -> bool {
  TCHAR buff[512];
  std::optional<std::string_view> item;

  while (amt > 0) {
    item = nextItem(buff);
    if (!item) {
      break;
    }
    pos_++;
    amt--;
  }

  if (item) {
    current_value_ = *item;
  }

  return amt == 0;
}

auto Playlist::nextItem(std::span<TCHAR> buf)
    -> std::optional<std::string_view> {
  while (file_open_ && !file_error_ && !f_eof(&file_)) {
    // FIXME: f_gets is quite slow (it does several very small reads instead of
    // grabbing a whole sector at a time), and it doesn't work well for very
    // long lines. We should do something smarter here.
    TCHAR* str = f_gets(buf.data(), buf.size(), &file_);
    if (str == NULL) {
      ESP_LOGW(kTag, "Error consuming playlist file at offset %llu",
               f_tell(&file_));
      file_error_ = true;
      return {};
    }

    std::string_view line{str};
    if (line.starts_with("#")) {
      continue;
    }
    if (line.ends_with('\n')) {
      line = line.substr(0, line.size() - 1);
    }
    return line;
  }

  // Got to EOF without reading a valid line.
  return {};
}

MutablePlaylist::MutablePlaylist(const std::string& playlistFilepath)
    : Playlist(playlistFilepath) {}

auto MutablePlaylist::clear() -> bool {
  std::unique_lock<std::mutex> lock(mutex_);

  // Try to recover from any IO errors.
  if (file_error_ && file_open_) {
    file_error_ = false;
    file_open_ = false;
    f_close(&file_);
  }

  FRESULT res;
  if (file_open_) {
    res = f_rewind(&file_);
    if (res != FR_OK) {
      ESP_LOGE(kTag, "error rewinding %u", res);
      file_error_ = true;
      return false;
    }
    res = f_truncate(&file_);
    if (res != FR_OK) {
      ESP_LOGE(kTag, "error truncating %u", res);
      file_error_ = true;
      return false;
    }
  } else {
    res = f_open(&file_, filepath_.c_str(),
                 FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
      ESP_LOGE(kTag, "error opening file %u", res);
      file_error_ = true;
      return false;
    }
    file_open_ = true;
  }

  total_size_ = 0;
  current_value_.clear();
  offset_cache_.clear();
  pos_ = -1;
  return true;
}

auto MutablePlaylist::append(Item i) -> void {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!file_open_ || file_error_) {
    return;
  }

  auto offset = f_tell(&file_);
  bool first_entry = current_value_.empty();

  // Seek to end and append
  auto end = f_size(&file_);
  auto res = f_lseek(&file_, end);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "Seek to end of file failed? Error %d", res);
    file_error_ = true;
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
    file_error_ = true;
    return;
  }
  res = f_sync(&file_);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "Failed to sync playlist file after append");
    file_error_ = true;
    return;
  }
}

}  // namespace audio
