/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "app_console.hpp"

#include <dirent.h>
#include <stdint.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>

#include "FreeRTOSConfig.h"
#include "audio_events.hpp"
#include "audio_fsm.hpp"
#include "database.hpp"
#include "esp_console.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "event_queue.hpp"
#include "ff.h"
#include "freertos/FreeRTOSConfig_arch.h"
#include "freertos/projdefs.h"
#include "index.hpp"
#include "track.hpp"

namespace console {

std::weak_ptr<database::Database> AppConsole::sDatabase;
audio::TrackQueue* AppConsole::sTrackQueue;

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

int CmdPlayFile(int argc, char** argv) {
  static const std::string usage = "usage: play [file or id]";
  if (argc < 2) {
    std::cout << usage << std::endl;
    return 1;
  }

  std::string path_or_id = argv[1];
  bool is_id = true;
  for (const auto& it : path_or_id) {
    if (!std::isdigit(it)) {
      is_id = false;
      break;
    }
  }

  if (is_id) {
    database::TrackId id = std::atoi(argv[1]);
    AppConsole::sTrackQueue->AddLast(id);
  } else {
    std::ostringstream path;
    path << '/' << argv[1];
    for (int i = 2; i < argc; i++) {
      path << ' ' << argv[i];
    }

    events::Audio().Dispatch(audio::PlayFile{.filename = path.str()});
  }

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

  std::shared_ptr<database::Result<database::IndexRecord>> res(
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
    if (res->values().at(choice).track()) {
      AppConsole::sTrackQueue->IncludeLast(
          std::make_shared<playlist::IndexRecordSource>(AppConsole::sDatabase,
                                                        res, 0, res, choice));
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
      std::cout << "\t(id:" << *r.track() << ")";
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

int CmdTaskStats(int argc, char** argv) {
  if (!configUSE_TRACE_FACILITY) {
    std::cout << "configUSE_TRACE_FACILITY must be enabled" << std::endl;
    std::cout << "also consider configTASKLIST_USE_COREID" << std::endl;
    return 1;
  }

  static const std::string usage = "usage: task_stats";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }

  // Pad the number of tasks so that uxTaskGetSystemState still returns info if
  // new tasks are started during measurement.
  size_t num_tasks = uxTaskGetNumberOfTasks() + 4;
  TaskStatus_t* start_status = new TaskStatus_t[num_tasks];
  TaskStatus_t* end_status = new TaskStatus_t[num_tasks];
  uint32_t start_elapsed_ticks = 0;
  uint32_t end_elapsed_ticks = 0;

  size_t start_num_tasks =
      uxTaskGetSystemState(start_status, num_tasks, &start_elapsed_ticks);

  vTaskDelay(pdMS_TO_TICKS(2500));

  size_t end_num_tasks =
      uxTaskGetSystemState(end_status, num_tasks, &end_elapsed_ticks);

  std::vector<std::pair<uint32_t, std::string>> info_strings;
  for (int i = 0; i < start_num_tasks; i++) {
    int k = -1;
    for (int j = 0; j < end_num_tasks; j++) {
      if (start_status[i].xHandle == end_status[j].xHandle) {
        k = j;
        break;
      }
    }

    if (k >= 0) {
      uint32_t run_time =
          end_status[k].ulRunTimeCounter - start_status[i].ulRunTimeCounter;

      float time_percent =
          static_cast<float>(run_time) /
          static_cast<float>(end_elapsed_ticks - start_elapsed_ticks);

      auto depth = uxTaskGetStackHighWaterMark2(start_status[i].xHandle);
      float depth_kib = static_cast<float>(depth) / 1024.0f;

      std::ostringstream str;
      str << start_status[i].pcTaskName;
      if (str.str().size() < 8) {
        str << "\t\t";
      } else {
        str << "\t";
      }

      if (configTASKLIST_INCLUDE_COREID) {
        if (start_status[i].xCoreID == tskNO_AFFINITY) {
          str << "any\t";
        } else {
          str << start_status[i].xCoreID << "\t";
        }
      }

      str << std::fixed << std::setprecision(1) << depth_kib;
      str << " KiB";
      if (depth_kib >= 10) {
        str << "\t";
      } else {
        str << "\t\t";
      }

      str << std::fixed << std::setprecision(1) << time_percent * 100;
      str << "%";

      info_strings.push_back({run_time, str.str()});
    }
  }

  std::sort(info_strings.begin(), info_strings.end(),
            [](const auto& first, const auto& second) {
              return first.first >= second.first;
            });

  std::cout << "name\t\t";
  if (configTASKLIST_INCLUDE_COREID) {
    std::cout << "core\t";
  }
  std::cout << "free stack\trun time" << std::endl;
  for (const auto& i : info_strings) {
    std::cout << i.second << std::endl;
  }

  delete[] start_status;
  delete[] end_status;

  return 0;
}

void RegisterTaskStates() {
  esp_console_cmd_t cmd{.command = "task_stats",
                        .help = "prints performance info for all tasks",
                        .hint = NULL,
                        .func = &CmdTaskStats,
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
  RegisterTaskStates();
}

}  // namespace console
