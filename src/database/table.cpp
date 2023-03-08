#include "table.hpp"

#include <memory>
#include <optional>

#include "esp32/himem.h"
#include "esp_err.h"
#include "ff.h"

namespace database {

const std::size_t kRamBlockSize = 32 * 1024;

auto Column::Open(std::string path) -> std::optional<Column> {
  FILINFO info;
  FRESULT res = f_stat(path.c_str(), &info);
  if (res != FR_OK) {
    return {};
  }

  FIL file;
  res = f_open(&file, path.c_str(), FA_READ | FA_WRITE);
  if (res != FR_OK) {
    return {};
  }

  return std::make_optional<Column>(file, info.fsize);
}

Column::Column(FIL file, std::size_t file_size)
    : file_(file), length_(file_size), loaded_range_(0, 0) {
  ESP_ERROR_CHECK(esp_himem_alloc(kRamBlockSize, &block_));
}

Column::~Column() {
  f_close(&file_);
  esp_himem_free(block_);
}

auto Column::ReadDataAtOffset(esp_himem_rangehandle_t range,
                              IndexOffset_t offset) -> RowData {
  // To start, we always need to map our address space.
  std::byte* paged_block;
  esp_himem_map(block_, range, 0, 0, kRamBlockSize, 0,
                reinterpret_cast<void**>(&paged_block));

  // Next, we need to see how long the data we're returning is. This might
  // already exist in memory.
  if (!IsOffsetLoaded(offset) ||
      !IsOffsetLoaded(offset + sizeof(std::size_t))) {
    LoadOffsetFromDisk({paged_block, kRamBlockSize}, offset);
  }

  IndexOffset_t paged_offset = offset - loaded_range_.first;
  std::size_t data_size =
      *(reinterpret_cast<std::size_t*>(paged_block + paged_offset));

  // Now that we have the size, we need to do the same thing again to get the
  // real data. Hopefully this doesn't require an actual second disk read, since
  // LoadOffsetFromDisk should load a generous amount after the offset we
  // previously gave.
  if (!IsOffsetLoaded(offset) || !IsOffsetLoaded(offset + data_size)) {
    LoadOffsetFromDisk({paged_block, kRamBlockSize}, offset);
  }

  paged_offset = offset - loaded_range_.first + sizeof(IndexOffset_t);
  cpp::span<std::byte> src(paged_block + paged_offset, data_size);

  auto res = std::make_unique<std::byte[]>(data_size);
  cpp::span<std::byte> dest(res.get(), data_size);

  std::copy(src.begin(), src.end(), dest.begin());

  // Finally, unmap from the range we were given to return it to its initial
  // state.
  esp_himem_unmap(range, paged_block, kRamBlockSize);

  return {std::move(res), data_size};
}

auto Column::AppendRow(cpp::span<std::byte> row) -> bool {
  FRESULT res = f_lseek(&file_, length_);
  if (res != FR_OK) {
    // TODO(jacqueline): Handle errors.
    return false;
  }

  std::size_t bytes_written = 0;
  std::size_t length = row.size_bytes();
  res = f_write(&file_, &length, sizeof(std::size_t), &bytes_written);
  if (res != FR_OK || bytes_written != sizeof(std::size_t)) {
    // TODO(jacqueline): Handle errors.
    return false;
  }

  res = f_write(&file_, row.data(), length, &bytes_written);
  if (res != FR_OK || bytes_written != length) {
    // TODO(jacqueline): Handle errors.
    return false;
  }

  length_ += sizeof(std::size_t) + row.size_bytes();

  return true;
}

auto Column::FlushChanges() -> void {
  f_sync(&file_);
}

auto Column::IsOffsetLoaded(IndexOffset_t offset) -> bool {
  return loaded_range_.first <= offset &&
         loaded_range_.second > offset + sizeof(std::size_t);
}

auto Column::LoadOffsetFromDisk(cpp::span<std::byte> dest, IndexOffset_t offset)
    -> bool {
  FRESULT res = f_lseek(&file_, offset);
  if (res != FR_OK) {
    // TODO(jacqueline): Handle errors.
    return false;
  }

  UINT bytes_read = 0;
  res = f_read(&file_, dest.data(), dest.size(), &bytes_read);
  if (res != FR_OK) {
    // TODO(jacqueline): Handle errors.
    return false;
  }

  loaded_range_.first = offset;
  loaded_range_.second = offset + bytes_read;

  return true;
}

}  // namespace database
