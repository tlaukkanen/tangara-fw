#include "app_console.hpp"

#include <dirent.h>
#include <cstdio>
#include <iostream>
#include <string>
#include "esp_console.h"

namespace console {

static AppConsole* sInstance = nullptr;

int CmdListDir(int argc, char** argv) {
  static const std::string usage = "usage: ls [directory]";
  if (argc > 2) {
    std::cout << usage << std::endl;
    return 1;
  }
  std::string path = drivers::kStoragePath;
  if (argc == 2) {
    path += "/";
    path += argv[1];
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

int CmdPlayFile(int argc, char** argv) {
  static const std::string usage = "usage: play [file]";
  if (argc != 2) {
    std::cout << usage << std::endl;
    return 1;
  }
  std::string path = drivers::kStoragePath;
  path += "/";
  path += argv[1];

  sInstance->playback_->Play(path.c_str());

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

  sInstance->playback_->Toggle();

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

AppConsole::AppConsole(std::unique_ptr<drivers::AudioPlayback> playback)
    : playback_(std::move(playback)) {
  sInstance = this;
}
AppConsole::~AppConsole() {
  sInstance = nullptr;
}

auto AppConsole::RegisterExtraComponents() -> void {
  RegisterListDir();
  RegisterPlayFile();
  RegisterToggle();
}

}  // namespace console
