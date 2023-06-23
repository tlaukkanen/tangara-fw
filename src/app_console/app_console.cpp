/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "app_console.hpp"

#include <dirent.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>

#include "audio_events.hpp"
#include "audio_fsm.hpp"
#include "database.hpp"
#include "esp_console.h"
#include "esp_log.h"
#include "event_queue.hpp"
#include "ff.h"
#include "index.hpp"
#include "track.hpp"

namespace console {

std::weak_ptr<database::Database> AppConsole::sDatabase;

int CmdListDir(int argc, char** argv) {
  auto lock = AppConsole::sDatabase.lock();
  if (lock == nullptr) {
    std::cout << "storage is not available" << std::endl;
    return 1;
  }

  std::string path;
  if (argc > 1) {
    std::ostringstream builder;
    builder << argv[1];
    for (int i = 2; i < argc; i++) {
      builder << ' ' << argv[i];
    }
    path = builder.str();
  } else {
    path = "";
  }

  FF_DIR dir;
  FRESULT res = f_opendir(&dir, path.c_str());
  if (res != FR_OK) {
    std::cout << "failed to open directory. does it exist?" << std::endl;
    return 1;
  }

  for (;;) {
    FILINFO info;
    res = f_readdir(&dir, &info);
    if (res != FR_OK || info.fname[0] == 0) {
      // No more files in the directory.
      break;
    } else {
      std::cout << path;
      if (!path.ends_with('/') && !path.empty()) {
        std::cout << '/';
      }
      std::cout << info.fname;
      if (info.fattrib & AM_DIR) {
        std::cout << '/';
      }
      std::cout << std::endl;
    }
  }

  f_closedir(&dir);

  return 0;
}

void RegisterListDir() {
  esp_console_cmd_t cmd{.command = "ls",
                        .help = "Lists SD contents",
                        .hint = NULL,
                        .func = &CmdListDir,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

// sInstance->playback_->Play(path + argv[1]);
int CmdPlayFile(int argc, char** argv) {
  static const std::string usage = "usage: play [file]";
  if (argc < 2) {
    std::cout << usage << std::endl;
    return 1;
  }

  std::ostringstream path;
  path << '/' << argv[1];
  for (int i = 2; i < argc; i++) {
    path << ' ' << argv[i];
  }

  events::Dispatch<audio::PlayFile, audio::AudioState>(
      audio::PlayFile{.filename = path.str()});

  return 0;
}

void RegisterPlayFile() {
  esp_console_cmd_t cmd{.command = "play",
                        .help = "Begins playback of the file at the given path",
                        .hint = "filepath",
                        .func = &CmdPlayFile,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

int CmdDbInit(int argc, char** argv) {
  static const std::string usage = "usage: db_init";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }

  auto db = AppConsole::sDatabase.lock();
  if (!db) {
    std::cout << "no database open" << std::endl;
    return 1;
  }
  db->Update();

  return 0;
}

void RegisterDbInit() {
  esp_console_cmd_t cmd{
      .command = "db_init",
      .help = "scans for playable files and adds them to the database",
      .hint = NULL,
      .func = &CmdDbInit,
      .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

int CmdDbTracks(int argc, char** argv) {
  static const std::string usage = "usage: db_tracks";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }

  auto db = AppConsole::sDatabase.lock();
  if (!db) {
    std::cout << "no database open" << std::endl;
    return 1;
  }
  std::unique_ptr<database::Result<database::Track>> res(
      db->GetTracks(20).get());
  while (true) {
    for (database::Track s : res->values()) {
      std::cout << s.tags()[database::Tag::kTitle].value_or("[BLANK]")
                << std::endl;
    }
    if (res->next_page()) {
      auto continuation = res->next_page().value();
      res.reset(db->GetPage(&continuation).get());
    } else {
      break;
    }
  }

  return 0;
}

void RegisterDbTracks() {
  esp_console_cmd_t cmd{.command = "db_tracks",
                        .help = "lists titles of ALL tracks in the database",
                        .hint = NULL,
                        .func = &CmdDbTracks,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

int CmdDbIndex(int argc, char** argv) {
  std::cout << std::endl;
  vTaskDelay(1);
  static const std::string usage = "usage: db_index [id] [choices ...]";

  auto db = AppConsole::sDatabase.lock();
  if (!db) {
    std::cout << "no database open" << std::endl;
    return 1;
  }

  auto indexes = db->GetIndexes();
  if (argc <= 1) {
    std::cout << usage << std::endl;
    std::cout << "available indexes:" << std::endl;
    std::cout << "id\tname" << std::endl;
    for (const database::IndexInfo& info : indexes) {
      std::cout << static_cast<int>(info.id) << '\t' << info.name << std::endl;
    }
    return 0;
  }

  int index_id = std::atoi(argv[1]);
  auto index = std::find_if(indexes.begin(), indexes.end(),
                            [=](const auto& i) { return i.id == index_id; });
  if (index == indexes.end()) {
    std::cout << "bad index id" << std::endl;
    return -1;
  }

  std::unique_ptr<database::Result<database::IndexRecord>> res(
      db->GetTracksByIndex(*index, 20).get());
  int choice_index = 2;

  if (res->values().empty()) {
    std::cout << "no entries for this index" << std::endl;
    return 1;
  }

  while (choice_index < argc) {
    int choice = std::atoi(argv[choice_index]);
    if (choice >= res->values().size()) {
      std::cout << "choice out of range" << std::endl;
      return -1;
    }
    auto cont = res->values().at(choice).Expand(20);
    if (!cont) {
      std::cout << "more choices than levels" << std::endl;
      return 0;
    }
    res.reset(db->GetPage<database::IndexRecord>(&*cont).get());
    choice_index++;
  }

  for (database::IndexRecord r : res->values()) {
    std::cout << r.text().value_or("<unknown>");
    if (r.track()) {
      std::cout << "\t(id:" << r.track()->data().id() << ")";
    }
    std::cout << std::endl;
  }

  if (res->next_page()) {
    std::cout << "(more results not shown)" << std::endl;
  }

  return 0;
}

void RegisterDbIndex() {
  esp_console_cmd_t cmd{.command = "db_index",
                        .help = "queries the database by index",
                        .hint = NULL,
                        .func = &CmdDbIndex,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

int CmdDbDump(int argc, char** argv) {
  static const std::string usage = "usage: db_dump";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }

  auto db = AppConsole::sDatabase.lock();
  if (!db) {
    std::cout << "no database open" << std::endl;
    return 1;
  }

  std::cout << "=== BEGIN DUMP ===" << std::endl;

  std::unique_ptr<database::Result<std::string>> res(db->GetDump(5).get());
  while (true) {
    for (std::string s : res->values()) {
      std::cout << s << std::endl;
    }
    if (res->next_page()) {
      auto continuation = res->next_page().value();
      res.reset(db->GetPage<std::string>(&continuation).get());
    } else {
      break;
    }
  }

  std::cout << "=== END DUMP ===" << std::endl;

  return 0;
}

void RegisterDbDump() {
  esp_console_cmd_t cmd{.command = "db_dump",
                        .help = "prints every key/value pair in the db",
                        .hint = NULL,
                        .func = &CmdDbDump,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

auto AppConsole::RegisterExtraComponents() -> void {
  RegisterListDir();
  RegisterPlayFile();
  /*
  RegisterToggle();
  RegisterVolume();
  RegisterAudioStatus();
  */
  RegisterDbInit();
  RegisterDbTracks();
  RegisterDbIndex();
  RegisterDbDump();
}

}  // namespace console
