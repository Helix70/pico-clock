#ifndef WIFISETUP_H
#define WIFISETUP_H

#include "settings.h"

/*
 * wifisetup_run - Interactive WiFi setup via USB serial console.
 *
 * Scans for networks, prompts the user to select one and enter a
 * password, connects, and saves credentials to NVS.  Loops until
 * a connection succeeds.  The user can type 0 to rescan.
 *
 * Returns 0 on success, negative errno on failure.
 */
int wifisetup_run(settings_t *s);

#endif
