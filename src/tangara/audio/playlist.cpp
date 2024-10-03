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

  if (!deserialiseCache()) {
    // Count the playlist size and build our offset cache.
    countItems();
    // Advance to the first item.
    skipToWithoutCache(0);
  }

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

// Serialise the cache to a file to avoid having to rescan
// the entire queue when resuming
auto Playlist::serialiseCache() -> bool {
  std::unique_lock<std::mutex> lock(mutex_);
  if (!file_open_) {
    return false;
  }

  FIL file;

  // Open the cache file
  std::string cache_file = filepath_ + ".cache";
  FRESULT res =
      f_open(&file, cache_file.c_str(), FA_READ | FA_WRITE | FA_CREATE_ALWAYS);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "failed to open cache file! res: %i", res);
    return false;
  }

  uint8_t header[12];

  // First 8 bytes = file size of queue file (for checking this file matches)
  uint64_t q_file_size = f_size(&file_);
  header[0] = q_file_size >> 56;
  header[1] = q_file_size >> 48;
  header[2] = q_file_size >> 40;
  header[3] = q_file_size >> 32;
  header[4] = q_file_size >> 24;
  header[5] = q_file_size >> 16;
  header[6] = q_file_size >> 8;
  header[7] = q_file_size;

  // Next 4 bytes = number of tracks in this queue
  header[8] = total_size_ >> 24;
  header[9] = total_size_ >> 16;
  header[10] = total_size_ >> 8;
  header[11] = total_size_;

  UINT bytes_written = 0;
  f_write(&file, header, 12, &bytes_written);
  if (bytes_written < 12) {
    return false;
  }

  // Next, write out every cached offset
  for (uint64_t offset : offset_cache_) {
    uint8_t bytes_out[8];
    bytes_out[0] = offset >> 56;
    bytes_out[1] = offset >> 48;
    bytes_out[2] = offset >> 40;
    bytes_out[3] = offset >> 32;
    bytes_out[4] = offset >> 24;
    bytes_out[5] = offset >> 16;
    bytes_out[6] = offset >> 8;
    bytes_out[7] = offset;
    f_write(&file, bytes_out, 8, &bytes_written);
    if (bytes_written < 8) {
      return false;
    }
  }

  f_close(&file);
  return true;
}

auto Playlist::deserialiseCache() -> bool {
  if (!file_open_) {
    return false;
  }

  FIL file;

  // Open the cache file
  std::string cache_file = filepath_ + ".cache";
  FRESULT res =
      f_open(&file, cache_file.c_str(), FA_READ | FA_WRITE | FA_OPEN_ALWAYS);
  if (res != FR_OK) {
    ESP_LOGE(kTag, "failed to open cache file! res: %i", res);
    return false;
  }

  uint8_t header[8];
  UINT bytes_read;
  f_read(&file, header, 12, &bytes_read);
  if (bytes_read != 12) {
    return false;
  }
  uint64_t file_size_check =
      header[0] << 56 | header[1] << 48 | header[2] << 40 | header[3] << 32 |
      header[4] << 24 | header[5] << 16 | header[6] << 8 | header[7];

  if (file_size_check != f_size(&file_)) {
    return false;
  }

  uint32_t size =
      header[8] << 24 | header[9] << 16 | header[10] << 8 | header[11];
  total_size_ = size;

  // Read in the cache
  uint8_t buf[8];
  size_t idx = 0;
  while (true) {
    f_read(&file, buf, 8, &bytes_read);
    if (bytes_read == 0) {
      break;
    }
    uint64_t offset = buf[0] << 56 | buf[1] << 48 | buf[2] << 40 |
                      buf[3] << 32 | buf[4] << 24 | buf[5] << 16 | buf[6] << 8 |
                      buf[7];
    offset_cache_.push_back(offset);
  }

  f_close(&file);
  return true;
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

auto MutablePlaylist::open() -> bool {
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

  auto queue_filesize = f_size(&file_);

  if (!deserialiseCache()) {
    // If there's no cache (or deserialising failed) and the queue is
    // sufficiently large, abort and clear the queue
    if (queue_filesize > 50000) {
      clear();
    } else {
      // Otherwise, read in the existing entries
      countItems();
      // Advance to the first item.
      skipToWithoutCache(0);
    }
  }

  return !file_error_;
  return false;
}

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
