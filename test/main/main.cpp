#include <stdio.h>
#include <cstdint>

#include "esp_console.h"
#include "esp_log.h"

#include "catch_runner.hpp"
#include "console.hpp"

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
 protected:
  virtual auto RegisterExtraComponents() -> void { RegisterCatch2(); }
  virtual auto GetStackSizeKiB() -> uint16_t {
    // Catch2 requires a particularly large stack.
    return 24;
  }
};

}  // namespace console

extern "C" void app_main(void) {
  esp_log_level_set("*", ESP_LOG_WARN);
  console::Console* c = new console::TestConsole();
  c->Launch();
}
