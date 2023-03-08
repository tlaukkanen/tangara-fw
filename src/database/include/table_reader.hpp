#pragma once

#include <string>

#include "result.hpp"
#include "span.hpp"

#include "table.hpp"

namespace database {

class TableReader {
 public:
  enum ReadError {
    OUT_OF_RANGE,
    IO_ERROR,
    PARSE_ERROR,
  };

  auto ReadEntryAtIndex(Index_t index) -> cpp::result<DatabaseEntry, ReadError>;

  template <typename T>
  auto ReadColumnOffsetAtIndex(Column<T> col, Index_t index)
      -> cpp::result<IndexOffset_t, ReadError>;

  template <typename T>
  auto ParseColumnAtIndex(Column<T> col, Index_t index)
      -> cpp::result<T, ReadError> {
    return ReadColumnOffsetAtIndex(col, index).map([&](IndexOffset_t offset) {
      return ReadColumnAtOffset(col, offset);
    });
  }

  template <typename T>
  auto ParseColumnAtOffset(Column<T> col, IndexOffset_t offset)
      -> cpp::result<T, ReadError> {
    return ReadDataAtOffset(col.Filename(), offset)
        .flat_map([&](cpp::span<std::byte> data) {
          auto res = = col.ParseValue(data);
          if (res) {
            return *res;
          } else {
            return cpp::fail(PARSE_ERROR);
          }
        });
  }

 private:
  auto ReadDataAtOffset(std::string filename, IndexOffset_t offset)
      -> cpp::span<std::byte>;
};

}  // namespace database
