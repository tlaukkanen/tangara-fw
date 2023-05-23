/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "app_console.hpp"

#include <dirent.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include "database.hpp"
#include "esp_console.h"
#include "esp_log.h"

namespace console {

static AppConsole* sInstance = nullptr;

std::string toSdPath(const std::string& filepath) {
  return std::string("/") + filepath;
}

int CmdListDir(int argc, char** argv) {
  static const std::string usage = "usage: ls [directory]";
  if (argc > 2) {
    std::cout << usage << std::endl;
    return 1;
  }

  auto lock = sInstance->database_.lock();
  if (lock == nullptr) {
    std::cout << "storage is not available" << std::endl;
    return 1;
  }

  std::string path;
  if (argc == 2) {
    path = toSdPath(argv[1]);
  } else {
    path = toSdPath("");
  }

  DIR* dir;
  struct dirent* ent;
  dir = opendir(path.c_str());
  while ((ent = readdir(dir))) {
    std::cout << ent->d_name << std::endl;
  }
  closedir(dir);

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

/*
int CmdPlayFile(int argc, char** argv) {
  static const std::string usage = "usage: play [file]";
  if (argc != 2) {
    std::cout << usage << std::endl;
    return 1;
  }

  std::string path = "/";
  sInstance->playback_->Play(path + argv[1]);

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

int CmdToggle(int argc, char** argv) {
  static const std::string usage = "usage: toggle";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }

  // sInstance->playback_->Toggle();

  return 0;
}

void RegisterToggle() {
  esp_console_cmd_t cmd{.command = "toggle",
                        .help = "Toggles between play and pause",
                        .hint = NULL,
                        .func = &CmdToggle,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

int CmdVolume(int argc, char** argv) {
  static const std::string usage = "usage: volume [0-255]";
  if (argc != 2) {
    std::cout << usage << std::endl;
    return 1;
  }

  long int raw_vol = strtol(argv[1], NULL, 10);
  if (raw_vol < 0 || raw_vol > 255) {
    std::cout << usage << std::endl;
    return 1;
  }

  // sInstance->playback_->SetVolume((uint8_t)raw_vol);

  return 0;
}

void RegisterVolume() {
  esp_console_cmd_t cmd{
      .command = "vol",
      .help = "Changes the volume (between 0 and 254. 255 is mute.)",
      .hint = NULL,
      .func = &CmdVolume,
      .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

int CmdAudioStatus(int argc, char** argv) {
  static const std::string usage = "usage: audio";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }

  sInstance->playback_->LogStatus();

  return 0;
}

void RegisterAudioStatus() {
  esp_console_cmd_t cmd{.command = "audio",
                        .help = "logs the current status of the audio pipeline",
                        .hint = NULL,
                        .func = &CmdAudioStatus,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}
*/

int CmdDbInit(int argc, char** argv) {
  static const std::string usage = "usage: db_init";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }

  auto db = sInstance->database_.lock();
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

int CmdDbSongs(int argc, char** argv) {
  static const std::string usage = "usage: db_songs";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }

  auto db = sInstance->database_.lock();
  if (!db) {
    std::cout << "no database open" << std::endl;
    return 1;
  }
  std::unique_ptr<database::Result<database::Song>> res(db->GetSongs(5).get());
  while (true) {
    for (database::Song s : res->values()) {
      std::cout << s.tags().title.value_or("[BLANK]") << std::endl;
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

void RegisterDbSongs() {
  esp_console_cmd_t cmd{.command = "db_songs",
                        .help = "lists titles of ALL songs in the database",
                        .hint = NULL,
                        .func = &CmdDbSongs,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

int CmdDbDump(int argc, char** argv) {
  static const std::string usage = "usage: db_dump";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }

  auto db = sInstance->database_.lock();
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

AppConsole::AppConsole(std::weak_ptr<database::Database> database)
    : database_(database) {
  sInstance = this;
}
AppConsole::~AppConsole() {
  sInstance = nullptr;
}

auto AppConsole::RegisterExtraComponents() -> void {
  RegisterListDir();
  /*
  RegisterPlayFile();
  RegisterToggle();
  RegisterVolume();
  RegisterAudioStatus();
  */
  RegisterDbInit();
  RegisterDbSongs();
  RegisterDbDump();
}

}  // namespace console