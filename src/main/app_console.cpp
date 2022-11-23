#include "app_console.hpp"

#include <dirent.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include "esp_console.h"

namespace console {

std::string toSdPath(std::string filepath) {
  return std::string(drivers::kStoragePath) + "/" + filepath;
}

int CmdListDir(int argc, char** argv) {
  static const std::string usage = "usage: ls [directory]";
  if (argc > 2) {
    std::cout << usage << std::endl;
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

int CmdPlayFile(int argc, char** argv) {
  static const std::string usage = "usage: play [file]";
  if (argc < 2 || argc > 3) {
    std::cout << usage << std::endl;
    return 1;
  }

  /*
  sInstance->playback_->Play(toSdPath(argv[1]));
  if (argc == 3) {
    sInstance->playback_->SetNextFile(toSdPath(argv[2]));
  }
  */

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

/*
AppConsole::AppConsole() {
  sInstance = this;
}
AppConsole::~AppConsole() {
  sInstance = nullptr;
}
*/

auto AppConsole::RegisterExtraComponents() -> void {
  RegisterListDir();
  RegisterPlayFile();
  RegisterToggle();
  RegisterVolume();
}

}  // namespace console
