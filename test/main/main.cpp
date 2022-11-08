#include <stdio.h>
#include <string.h>

#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"

#include "catch_runner.hpp"

void register_catch2() {
  esp_console_cmd_t cmd{
      .command = "catch",
      .help = "Execute the catch2 test runner. Use -? for options.",
      .hint = NULL,
      .func = &exec_catch2,
      .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

extern "C" void app_main(void) {
  esp_console_repl_t* repl = nullptr;
  esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
  repl_config.max_history_len = 16;
  repl_config.prompt = " →";
  repl_config.max_cmdline_length = 256;
  // Catch2 needs a huge stack, since it does a lot of pretty string formatting.
  repl_config.task_stack_size = 1024 * 24;

  esp_console_dev_uart_config_t hw_config =
      ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

  esp_console_register_help_command();
  register_catch2();

  ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
