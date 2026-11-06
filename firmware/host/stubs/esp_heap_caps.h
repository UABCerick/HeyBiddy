#pragma once
#include <stddef.h>
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_SPIRAM 2
static inline size_t heap_caps_get_free_size(int caps) { return caps == MALLOC_CAP_SPIRAM ? 8000 * 1024 : 250 * 1024; }
