/*
 * Copyright 2023 ailurux <ailuruxx@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once 

#include <string>
#include <optional>

#include "ff.h"

namespace lua {

// Note for when reading FILINFO, that we are in LFN mode:
// http://elm-chan.org/fsw/ff/doc/sfileinfo.html
struct FileEntry {
    int index;
    bool isHidden;
    bool isDirectory;
    bool isTrack;
    std::string filepath;
    std::string name;
};

class FileIterator {
 public:
  FileIterator(std::string filepath, bool showHidden);
  ~FileIterator();

  auto value() const -> const std::optional<FileEntry>&;
  auto next() -> void;
  auto prev() -> void;

 private: 
  FF_DIR dir_;
  std::string original_path_;
  bool show_hidden_;

  std::optional<FileEntry> current_;
  int offset_;

  auto iterate(bool reverse = false) -> bool;
};

} // namespace lua