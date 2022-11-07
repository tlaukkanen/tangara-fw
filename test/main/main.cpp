#include <stdio.h>
#include <string.h>

static void print_banner(const char* text);

extern "C" {
  void app_main(void)
  {
      print_banner("Running tests without [ignore] tag");
  }
}

static void print_banner(const char* text)
{
    printf("\n#### %s #####\n\n", text);
}

