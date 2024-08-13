/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "dev_console/console.hpp"
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <string>

#include "esp_console.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "esp_system.h"

#include "memory_resource.hpp"

namespace console {

int CmdLogLevel(int argc, char** argv) {
  static const std::pmr::string usage =
      "usage: loglevel [tag] [VERBOSE,DEBUG,INFO,WARN,ERROR,NONE]";
  if (argc < 2 || argc > 3) {
    std::cout << usage << std::endl;
    return 1;
  }

  std::string tag;
  if (argc == 2) {
    tag = "*";
  } else {
    tag = argv[1];
  }

  std::string raw_level;
  if (argc == 2) {
    raw_level = argv[1];
  } else {
    raw_level = argv[2];
  }
  std::transform(raw_level.begin(), raw_level.end(), raw_level.begin(),
                 [](unsigned char c) { return std::toupper(c); });

  esp_log_level_t level;
  if (raw_level == "VERBOSE") {
    level = ESP_LOG_VERBOSE;
  } else if (raw_level == "DEBUG") {
    level = ESP_LOG_DEBUG;
  } else if (raw_level == "INFO") {
    level = ESP_LOG_INFO;
  } else if (raw_level == "WARN") {
    level = ESP_LOG_WARN;
  } else if (raw_level == "ERROR") {
    level = ESP_LOG_ERROR;
  } else if (raw_level == "NONE") {
    level = ESP_LOG_NONE;
  } else {
    std::cout << usage << std::endl;
    return 1;
  }

  esp_log_level_set(tag.c_str(), level);

  return 0;
}

void RegisterLogLevel() {
  esp_console_cmd_t cmd{
      .command = "loglevel",
      .help =
          "Sets the log level to one of \"VERBOSE\", \"DEBUG\", \"INFO\", "
          "\"WARN\", \"ERROR\", \"NONE\"",
      .hint = "level",
      .func = &CmdLogLevel,
      .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

int CmdInterrupts(int argc, char** argv) {
  static const std::pmr::string usage = "usage: intr";
  if (argc != 1) {
    std::cout << usage << std::endl;
    return 1;
  }
  esp_intr_dump(NULL);
  return 0;
}

void RegisterInterrupts() {
  esp_console_cmd_t cmd{.command = "intr",
                        .help = "Dumps a table of all allocated interrupts",
                        .hint = NULL,
                        .func = &CmdInterrupts,
                        .argtable = NULL};
  esp_console_cmd_register(&cmd);
  cmd.command = "interrupts";
  esp_console_cmd_register(&cmd);
}

Console::Console() {}
Console::~Console() {}

auto Console::RegisterCommonComponents() -> void {
  esp_console_register_help_command();
  RegisterLogLevel();
  RegisterInterrupts();
}

static Console* sInstance;

static auto prerun_cb() -> void {
  if (sInstance) {
    sInstance->PrerunCallback();
  }
}

auto Console::PrerunCallback() -> void {
  puts("\r\nPress any key to enter dev console.\r\n");
  setvbuf(stdin, NULL, _IONBF, 0);
  fgetc(stdin);
}

auto Console::Launch() -> void {
  sInstance = this;
  esp_console_repl_t* repl = nullptr;
  esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
  repl_config.max_history_len = 16;
  repl_config.prompt = " →";
  repl_config.max_cmdline_length = 256;
  repl_config.task_stack_size = 1024 * GetStackSizeKiB();
  repl_config.prerun_cb = prerun_cb;

  esp_console_dev_uart_config_t hw_config =
      ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

  RegisterCommonComponents();
  RegisterExtraComponents();

  ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

}  // namespace console
