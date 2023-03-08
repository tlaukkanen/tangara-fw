#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "esp32/himem.h"
#include "ff.h"
#include "span.hpp"
#include "sys/_stdint.h"

namespace database {

// Types used for indexing into files on disk. These should, at minimum, match
// the size of the types that the underlying filesystem uses to address within
// files. FAT32 uses 32 bit address. If we drop this and just support exFAT, we
// can change these to 64 bit types.
typedef uint32_t Index_t;
typedef Index_t IndexOffset_t;

// The amount of memory that will be used to page database columns in from disk.
// Currently we only use a single 'page' in PSRAM per column, but with some
// refactoring we could easily page more.
// Keep this value 32KiB-aligned for himem compatibility.
extern const std::size_t kRamBlockSize;

struct DatabaseHeader {
  uint32_t magic_number;
  uint16_t db_version;
  Index_t num_indices;
};

struct DatabaseEntry {
  uint8_t type;
  std::string path;

  std::string title;
  std::string album;
  std::string artist;
  std::string album_artist;
};

struct IndexEntry {
  uint8_t type;
  IndexOffset_t path;

  IndexOffset_t title;
  IndexOffset_t album;
  IndexOffset_t artist;
  IndexOffset_t album_artist;
};

struct RowData {
  std::unique_ptr<std::byte[]> arr;
  std::size_t length;
};

// Representation of a single column of data. Each column is simply a tightly
// packed list of [size, [bytes, ...]] pairs. Callers are responsible for
// parsing and encoding the actual bytes themselves.
class Column {
 public:
  static auto Open(std::string) -> std::optional<Column>;

  Column(FIL file, std::size_t file_size);
  ~Column();

  auto ReadDataAtOffset(esp_himem_rangehandle_t, IndexOffset_t) -> RowData;
  auto AppendRow(cpp::span<std::byte> row) -> bool;
  auto FlushChanges() -> void;

 private:
  FIL file_;
  IndexOffset_t length_;

  esp_himem_handle_t block_;
  std::pair<IndexOffset_t, IndexOffset_t> loaded_range_;

  auto IsOffsetLoaded(IndexOffset_t offset) -> bool;
  auto LoadOffsetFromDisk(cpp::span<std::byte> dest, IndexOffset_t offset)
      -> bool;
};

}  // namespace database
