/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <iomanip>
#include <ostream>

#include <string>
#include "span.hpp"

namespace util {

inline std::string format_hex_string(cpp::span<const std::byte> data) {
  std::ostringstream oss;
  std::ostringstream ascii_values;
  int count = 0;
  for (auto byte : data) {
    if (count % 16 == 0) {
      if (ascii_values.str().size() > 0) {
        oss << "\t|" << ascii_values.str() << "|";
        // Reset ascii values
        ascii_values.str("");
      }
      oss << std::endl;
      oss << "0x" << std::uppercase << std::setfill('0') << std::setw(2)
          << std::hex << count << '\t';
    } else if (count % 8 == 0) {
      oss << " ";
    }
    int byte_val = (int)byte;
    oss << "[0x" << std::uppercase << std::setfill('0') << std::setw(2)
        << std::hex << byte_val << ']';
    if (byte_val >= 32 && byte_val < 127) {
      ascii_values << (char)byte;
    } else {
      ascii_values << ".";
    }
    count++;
  }
  return oss.str();
}

}  // namespace util
