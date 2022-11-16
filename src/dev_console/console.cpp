#include "console.hpp"
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <iostream>
#include <string>

#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"

namespace console {

int CmdLogLevel(int argc, char** argv) {
  static const std::string usage =
      "usage: loglevel [VERBOSE,DEBUG,INFO,WARN,ERROR,NONE]";
  if (argc != 2) {
    std::cout << usage << std::endl;
    return 1;
  }
  std::string level_str = argv[1];
  std::transform(level_str.begin(), level_str.end(), level_str.begin(),
                 [](unsigned char c) { return std::toupper(c); });

  esp_log_level_t level;
  if (level_str == "VERBOSE") {
    level = ESP_LOG_VERBOSE;
  } else if (level_str == "DEBUG") {
    level = ESP_LOG_DEBUG;
  } else if (level_str == "INFO") {
    level = ESP_LOG_INFO;
  } else if (level_str == "WARN") {
    level = ESP_LOG_WARN;
  } else if (level_str == "ERROR") {
    level = ESP_LOG_ERROR;
  } else if (level_str == "NONE") {
    level = ESP_LOG_NONE;
  } else {
    std::cout << usage << std::endl;
    return 1;
  }

  esp_log_level_set("*", level);

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

Console::Console() {}
Console::~Console() {}

auto Console::RegisterCommonComponents() -> void {
  esp_console_register_help_command();
  RegisterLogLevel();
}

auto Console::Launch() -> void {
  esp_console_repl_t* repl = nullptr;
  esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
  repl_config.max_history_len = 16;
  repl_config.prompt = " →";
  repl_config.max_cmdline_length = 256;
  repl_config.task_stack_size = 1024 * GetStackSizeKiB();

  esp_console_dev_uart_config_t hw_config =
      ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

  RegisterCommonComponents();
  RegisterExtraComponents();

  ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

}  // namespace console
