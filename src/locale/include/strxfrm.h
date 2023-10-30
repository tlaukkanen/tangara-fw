/*
 * Copyright 2023 jacqueline <me@jacqueline.id.au>
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint_fast32_t nrules;
  unsigned char* rulesets;
  unsigned char* weights;
  int32_t* table;
  unsigned char* extra;
  int32_t* indirect;
} locale_data_t;

bool parse_locale_data(const void* raw_data, size_t size, locale_data_t* out);

size_t glib_strxfrm(char* dest,
                    const char* src,
                    size_t n,
                    locale_data_t* locale);

#ifdef __cplusplus
}
#endif
