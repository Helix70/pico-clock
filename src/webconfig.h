#ifndef WEBCONFIG_H
#define WEBCONFIG_H

#include "settings.h"

typedef void (*webconfig_callback_t)(const settings_t *settings);

int webconfig_start(settings_t *settings, webconfig_callback_t on_save);
void webconfig_stop(void);

#endif
