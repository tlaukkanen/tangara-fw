#pragma once

#include "esp_heap_caps.h"
#include "opus_defines.h"

#define OVERRIDE_OPUS_ALLOC
#define OVERRIDE_OPUS_FREE

static OPUS_INLINE void *opus_alloc (size_t size)
{
   return heap_caps_malloc(size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
}

static OPUS_INLINE void opus_free (void *ptr)
{
   heap_caps_free(ptr);
}
