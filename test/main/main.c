#include <stdio.h>
#include <string.h>
#include "unity.h"

static void print_banner(const char* text);

void app_main(void)
{
    print_banner("Running tests without [ignore] tag");
    UNITY_BEGIN();
    unity_run_tests_by_tag("[ignore]", true);
    UNITY_END();

    print_banner("Starting interactive test menu");
    unity_run_menu();
}

static void print_banner(const char* text)
{
    printf("\n#### %s #####\n\n", text);
}

