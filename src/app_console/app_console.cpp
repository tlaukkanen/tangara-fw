/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "app_console.hpp"

#include <dirent.h>
#include <stdint.h>
#include <sys/_stdint.h>

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
#include "esp_app_desc.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_heap_trace.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "event_queue.hpp"
#include "ff.h"
#include "freertos/FreeRTOSConfig_arch.h"
#include "freertos/projdefs.h"
#include "haptics.hpp"
#include "index.hpp"
#include "lua_thread.hpp"
#include "memory_resource.hpp"
#include "samd.hpp"
#include "service_locator.hpp"
#include "system_events.hpp"
#include "track.hpp"
#include "ui_events.hpp"

namespace console {

std::shared_ptr<system_fsm::ServiceLocator> AppConsole::sServices;

int CmdVersion(int argc, char** argv) {
  std::cout << "firmware-version=" << esp_app_get_description()->version << std::endl;
  std::cout << "samd-version=" << AppConsole::sServices->samd().Version() << std::endl;
  std::cout << "collation=" << AppConsole::sServices->collator().Describe().value_or("") << std::endl;
  std::cout << "database-schema=" << database::kCurrentDbVersion << std::endl;
  return 0;
}

void RegisterVersion() {
  esp_console_cmd_t cmd{.command = "version",
                        .help = "Displays firmware version information",
                        .hint = NULL,
                        .func = &CmdVersion,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

int CmdListDir(int argc, char** argv) {
  auto db = AppConsole::sServices->database().lock();
  if (!db) {
    std::cout << "storage is not available" << std::endl;
    return 1;
  }

  std::pmr::string path;
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
  static const std::pmr::string usage = "usage: play [file or id]";
  if (argc < 2) {
    std::cout << usage << std::endl;
    return 1;
  }

  std::pmr::string path_or_id = argv[1];
  bool is_id = true;
  for (const auto& it : path_or_id) {
    if (!std::isdigit(it)) {
      is_id = false;
      break;
    }
  }

  if (is_id) {
    database::TrackId id = std::atoi(argv[1]);
    AppConsole::sServices->track_queue().append(id);
  } else {
    std::pmr::string path{&memory::kSpiRamResource};
    path += '/';
    path += argv[1];
    for (int i = 2; i < argc; i++) {
      path += ' ';
      path += argv[i];
    }

    events::Audio().Dispatch(
        audio::PlayFile{.filename = {path.data(), path.size()}});
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
  static const std::pmr::string usage = "usage: db_init";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }

  auto db = AppConsole::sServices->database().lock();
  if (!db) {
    std::cout << "no database open" << std::endl;
    return 1;
  }

  AppConsole::sServices->bg_worker().Dispatch<void>(
      [=]() { db->updateIndexes(); });

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

int CmdTasks(int argc, char** argv) {
#if (configUSE_TRACE_FACILITY == 0)
  std::cout << "configUSE_TRACE_FACILITY must be enabled" << std::endl;
  std::cout << "also consider configTASKLIST_USE_COREID" << std::endl;
  return 1;
#endif

  static const std::pmr::string usage = "usage: tasks";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }

  // Pad the number of tasks so that uxTaskGetSystemState still returns info
  // if new tasks are started during measurement.
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

  std::vector<std::pair<uint32_t, std::pmr::string>> info_strings;
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

      std::ostringstream str{};
      str << start_status[i].pcTaskName;
      if (str.str().size() < 8) {
        str << "\t\t";
      } else {
        str << "\t";
      }

#if (configTASKLIST_INCLUDE_COREID == 1)
      if (start_status[i].xCoreID == tskNO_AFFINITY) {
        str << "any\t";
      } else {
        str << start_status[i].xCoreID << "\t";
      }
#endif

      str << std::fixed << std::setprecision(1) << depth_kib;
      str << " KiB";
      if (depth_kib >= 10) {
        str << "\t";
      } else {
        str << "\t\t";
      }

      str << std::fixed << std::setprecision(1) << (time_percent * 100);
      str << "%";

      info_strings.push_back({run_time, std::pmr::string{str.str()}});
    }
  }

  std::sort(info_strings.begin(), info_strings.end(),
            [](const auto& first, const auto& second) {
              return first.first >= second.first;
            });

  std::cout << "name\t\t";
#if (configTASKLIST_INCLUDE_COREID == 1)
  std::cout << "core\t";
#endif
  std::cout << "free stack\trun time" << std::endl;
  for (const auto& i : info_strings) {
    std::cout << i.second << std::endl;
  }

  delete[] start_status;
  delete[] end_status;

  return 0;
}

void RegisterTasks() {
  esp_console_cmd_t cmd{.command = "tasks",
                        .help = "prints performance info for all tasks",
                        .hint = NULL,
                        .func = &CmdTasks,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

int CmdHeaps(int argc, char** argv) {
  static const std::pmr::string usage = "usage: heaps";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }

  std::cout << "heap stats (total):" << std::endl;
  std::cout << (esp_get_free_heap_size() / 1024) << " KiB free" << std::endl;
  std::cout << (esp_get_minimum_free_heap_size() / 1024)
            << " KiB free at lowest" << std::endl;

  std::cout << "heap stats (internal):" << std::endl;
  std::cout << (heap_caps_get_free_size(MALLOC_CAP_DMA) / 1024) << " KiB free"
            << std::endl;
  std::cout << (heap_caps_get_minimum_free_size(MALLOC_CAP_DMA) / 1024)
            << " KiB free at lowest" << std::endl;

  std::cout << "heap stats (external):" << std::endl;
  std::cout << (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024)
            << " KiB free" << std::endl;
  std::cout << (heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM) / 1024)
            << " KiB free at lowest" << std::endl;

  return 0;
}

void RegisterHeaps() {
  esp_console_cmd_t cmd{.command = "heaps",
                        .help = "prints free heap space",
                        .hint = NULL,
                        .func = &CmdHeaps,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

int CmdStacks(int argc, char** argv) {
  static const std::pmr::string usage = "usage: stacks";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }
  events::Ui().Dispatch(ui::DumpLuaStack{});
  return 0;
}

void RegisterStacks() {
  esp_console_cmd_t cmd{.command = "stacks",
                        .help = "prints contents of each lua stack",
                        .hint = NULL,
                        .func = &CmdStacks,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

#if CONFIG_HEAP_TRACING
static heap_trace_record_t* sTraceRecords = nullptr;
static bool sIsTracking = false;

int CmdAllocs(int argc, char** argv) {
  static const std::pmr::string usage = "usage: allocs";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }

  if (sTraceRecords == nullptr) {
    constexpr size_t kNumRecords = 256;
    sTraceRecords = reinterpret_cast<heap_trace_record_t*>(heap_caps_calloc(
        kNumRecords, sizeof(heap_trace_record_t), MALLOC_CAP_DMA));
    ESP_ERROR_CHECK(heap_trace_init_standalone(sTraceRecords, kNumRecords));
  }

  if (!sIsTracking) {
    std::cout << "tracking allocs" << std::endl;
    sIsTracking = true;
    ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));
  } else {
    sIsTracking = false;
    ESP_ERROR_CHECK(heap_trace_stop());
    heap_trace_dump();
  }

  return 0;
}

void RegisterAllocs() {
  esp_console_cmd_t cmd{.command = "allocs",
                        .help = "",
                        .hint = NULL,
                        .func = &CmdAllocs,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}
#endif

int CmdBtList(int argc, char** argv) {
  static const std::pmr::string usage = "usage: bt_list <index>";
  if (argc > 2) {
    std::cout << usage << std::endl;
    return 1;
  }

  auto devices = AppConsole::sServices->bluetooth().KnownDevices();
  if (argc == 2) {
    int index = std::atoi(argv[1]);
    if (index < 0 || index >= devices.size()) {
      std::cout << "index out of range" << std::endl;
      return -1;
    }
    AppConsole::sServices->bluetooth().SetPreferredDevice(
        devices[index].address);
  } else {
    std::cout << "mac\t\trssi\tname" << std::endl;
    for (const auto& device : devices) {
      for (size_t i = 0; i < device.address.size(); i++) {
        std::cout << std::hex << std::setfill('0') << std::setw(2)
                  << static_cast<int>(device.address[i]);
      }
      float perc =
          (static_cast<double>(device.signal_strength) + 127.0) / 256.0 * 100;
      std::cout << "\t" << std::fixed << std::setprecision(0) << perc << "%";
      std::cout << "\t" << device.name << std::endl;
    }
  }

  return 0;
}

void RegisterBtList() {
  esp_console_cmd_t cmd{.command = "bt_list",
                        .help = "lists and connects to bluetooth devices",
                        .hint = NULL,
                        .func = &CmdBtList,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

int CmdSamd(int argc, char** argv) {
  static const std::pmr::string usage = "usage: samd [flash|charge|off]";
  if (argc != 2) {
    std::cout << usage << std::endl;
    return 1;
  }
  drivers::Samd& samd = AppConsole::sServices->samd();

  std::pmr::string cmd{argv[1]};
  if (cmd == "flash") {
    std::cout << "resetting samd..." << std::endl;
    vTaskDelay(pdMS_TO_TICKS(5));
    samd.ResetToFlashSamd();
  } else if (cmd == "charge") {
    auto res = samd.GetChargeStatus();
    if (res) {
      switch (res.value()) {
        case drivers::Samd::ChargeStatus::kNoBattery:
          std::cout << "kNoBattery" << std::endl;
          break;
        case drivers::Samd::ChargeStatus::kBatteryCritical:
          std::cout << "kBatteryCritical" << std::endl;
          break;
        case drivers::Samd::ChargeStatus::kDischarging:
          std::cout << "kDischarging" << std::endl;
          break;
        case drivers::Samd::ChargeStatus::kChargingRegular:
          std::cout << "kChargingRegular" << std::endl;
          break;
        case drivers::Samd::ChargeStatus::kChargingFast:
          std::cout << "kChargingFast" << std::endl;
          break;
        case drivers::Samd::ChargeStatus::kFullCharge:
          std::cout << "kFullCharge" << std::endl;
          break;
      }
    } else {
      std::cout << "unknown" << std::endl;
    }
  } else if (cmd == "msc") {
    bool current = samd.UsbMassStorage();
    std::cout << "toggling to: " << !current << std::endl;
    events::System().Dispatch(system_fsm::SamdUsbMscChanged{.en = !current});
  } else if (cmd == "off") {
    std::cout << "bye !!!" << std::endl;
    vTaskDelay(pdMS_TO_TICKS(5));
    AppConsole::sServices->samd().PowerDown();
  } else {
    std::cout << usage << std::endl;
    return 1;
  }

  return 0;
}

void RegisterSamd() {
  esp_console_cmd_t cmd{.command = "samd",
                        .help = "",
                        .hint = NULL,
                        .func = &CmdSamd,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

int CmdCoreDump(int argc, char** argv) {
  static const std::pmr::string usage = "usage: core_dump";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }

  abort();

  return 0;
}

void RegisterCoreDump() {
  esp_console_cmd_t cmd{.command = "core_dump",
                        .help = "",
                        .hint = NULL,
                        .func = &CmdCoreDump,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

int CmdHaptics(int argc, char** argv) {
  static const std::pmr::string usage =
      "There are 123 waveform effects, and 5 'libraries' (motor types);\n"
      "see the DRV2624 datasheet for more details.\n\n"
      "Usages:\n"
      "  haptic_effect\n"
      "  haptic_effect library\n"
      "  haptic_effect from-effect to-effect\n"
      "  haptic_effect from-effect to-effect library\n"
      "eg.\n"
      "  haptic_effect        (plays from 1 to 123 with the default "
      "library)\n"
      "  haptic_effect 3      (plays from 1 to 123 with library 3\n"
      "  haptic_effect 1 100  (plays from 1 to 100 with the default "
      "library)\n"
      "  haptic_effect 1 10 4 (plays from 1 to 10 with library 4)";

  auto& haptics = AppConsole::sServices->haptics();

  if (argc == 1) {
    haptics.TourEffects();

  } else if (argc == 2 && argv[1] != std::string{"help"}) {
    std::istringstream raw_library_id{argv[1]};
    int library_id = 0;
    raw_library_id >> library_id;

    haptics.TourEffects(static_cast<drivers::Haptics::Library>(library_id));

  } else if (argc == 3) {
    std::istringstream raw_effect_from_id{argv[1]};
    std::istringstream raw_effect_to_id{argv[2]};
    int effect_from_id, effect_to_id = 0;
    raw_effect_from_id >> effect_from_id;
    raw_effect_to_id >> effect_to_id;

    haptics.TourEffects(static_cast<drivers::Haptics::Effect>(effect_from_id),
                        static_cast<drivers::Haptics::Effect>(effect_to_id));

  } else if (argc == 4) {
    std::istringstream raw_effect_from_id{argv[1]};
    std::istringstream raw_effect_to_id{argv[2]};
    std::istringstream raw_library_id{argv[3]};
    int effect_from_id, effect_to_id, library_id = 0;
    raw_effect_from_id >> effect_from_id;
    raw_effect_to_id >> effect_to_id;
    raw_library_id >> library_id;

    haptics.TourEffects(static_cast<drivers::Haptics::Effect>(effect_from_id),
                        static_cast<drivers::Haptics::Effect>(effect_to_id),
                        static_cast<drivers::Haptics::Library>(library_id));
  } else {
    std::cout << usage << std::endl;
    return 1;
  }
  return 0;
}

void RegisterHapticEffect() {
  esp_console_cmd_t cmd{
      .command = "haptic_effect",
      .help =
          "Plays one, a range of, or all effects from a DRV2624 effect "
          "library; run 'haptic_effect help' for more.",
      .hint = NULL,
      .func = &CmdHaptics,
      .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

static const char kReplMain[] =
    "package.path = '/repl/?.lua;/repl/?/init.lua;' .. package.path\n"
    "local repl = require 'repl.console'\n"
    "local col = require('term').colors\n"
    "function repl:getprompt(level)\n"
    "if level == 1 then\n"
    "return col.blue .. '>>' .. col.reset\n"
    "else\n"
    "return '..'\n"
    "end\n"
    "end\n"
    "repl:loadplugin 'linenoise'\n"
    "repl:loadplugin 'history'\n"
    "repl:loadplugin 'completion'\n"
    "repl:loadplugin 'autoreturn'\n"
    "repl:loadplugin 'pretty_print'\n"
    "print 'Lua 5.4.4  Copyright (C) 1994-2023 Lua.org, PUC-Rio'\n"
    "print 'luarepl 0.10 Copyright (C) 2011-2015 Rob Hoelz'\n"
    "repl:run()\n";

int CmdLua(int argc, char** argv) {
  std::unique_ptr<lua::LuaThread> context{
      lua::LuaThread::Start(*AppConsole::sServices)};
  if (!context) {
    return 1;
  }

  if (argc == 1) {
    return context->RunString(kReplMain);
  } else {
    std::ostringstream path;
    path << argv[0];
    for (size_t i = 1; i < argc; i++) {
      path << " " << argv[i];
    }
    FILINFO info;
    if (f_stat(path.str().c_str(), &info) != FR_OK) {
      std::cout << "file not found: " << path.str() << std::endl;
    }
    return context->RunScript(path.str());
  }
  return 0;
}

void RegisterLua() {
  esp_console_cmd_t cmd{
      .command = "lua",
      .help = "Executes a lua script. With no args, begins a lua repl session",
      .hint = NULL,
      .func = &CmdLua,
      .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

auto AppConsole::RegisterExtraComponents() -> void {
  RegisterVersion();
  RegisterListDir();
  RegisterPlayFile();
  /*
  RegisterToggle();
  RegisterVolume();
  RegisterAudioStatus();
  */
  RegisterDbInit();
  RegisterTasks();

  RegisterHeaps();
  RegisterStacks();

#if CONFIG_HEAP_TRACING
  RegisterAllocs();
#endif

  RegisterBtList();
  RegisterSamd();
  RegisterCoreDump();

  RegisterHapticEffect();
  RegisterLua();
}

}  // namespace console
