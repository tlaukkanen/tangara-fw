#include "catch_runner.hpp"

#define CATCH_CONFIG_RUNNER
#include "catch2/catch.hpp"

#include <stdio.h>
#include <string.h>
#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"

// There must be exactly on Session instance at all times; attempting to destroy
// this will result in memory corruption.
static Catch::Session sCatchSession;

int exec_catch2(int argc, char** argv) {
  // Reset the existing configuration before applying a new one. Otherwise we
  // will get the combination of all configs.
  sCatchSession.configData() = Catch::ConfigData();

  int result = sCatchSession.applyCommandLine(argc, argv);
  if (result != 0) {
    return result;
  }

  // Returns number of failures.
  int failures = sCatchSession.run();

  return failures > 0;
}
