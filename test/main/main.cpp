/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/unistd.h>

#include <cstdint>

#include "freertos/FreeRTOS.h"

#include "catch_runner.hpp"
#include "esp_console.h"
#include "esp_log.h"

#include "dev_console/console.hpp"

void RegisterCatch2() {
  esp_console_cmd_t cmd{
      .command = "catch",
      .help = "Execute the catch2 test runner. Use -? for options.",
      .hint = NULL,
      .func = &exec_catch2,
      .argtable = NULL};
  esp_console_cmd_register(&cmd);
}

namespace console {

class TestConsole : public Console {
 public:
  virtual auto PrerunCallback() -> void {
    char* arg = "catch";
    exec_catch2(1, &arg);
    Console::PrerunCallback();
  }

 protected:
  virtual auto RegisterExtraComponents() -> void { RegisterCatch2(); }
  virtual auto GetStackSizeKiB() -> uint16_t {
    // Catch2 requires a particularly large stack to begin with, and some of the
    // tests (*cough*libmad*cough*) also use a lot of stack.
    return 64;
  }
};

}  // namespace console

extern "C" void app_main(void) {
  esp_log_level_set("*", ESP_LOG_WARN);
  console::Console* c = new console::TestConsole();
  c->Launch();
}
