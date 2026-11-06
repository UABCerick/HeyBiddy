#pragma once
/* El menu circular. Es una app mas para el gestor, pero su pantalla nunca se borra. */
#include "app.h"

const app_t *launcher_app(void);
void launcher_build(lv_obj_t *screen);
