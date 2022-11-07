#include "catch_runner.hpp"

#define CATCH_CONFIG_RUNNER
#include "catch2/catch.hpp"

void run_catch(void) {
  int argc = 1;
  char *argv = "catch2";
    Catch::Session().run( argc, &argv );
}
