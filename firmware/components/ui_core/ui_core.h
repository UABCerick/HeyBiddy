#pragma once
/* Solo para main.c: arranca el sistema con la lista de apps de registry.c */
#include <stddef.h>
#include "app.h"

void ui_core_start(const app_t *const *apps, size_t count);
