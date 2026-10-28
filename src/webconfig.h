#ifndef WEBCONFIG_H
#define WEBCONFIG_H

#include "settings.h"

#define WEBCONFIG_AP_SSID  "HU058-Clock"

typedef void (*webconfig_callback_t)(const settings_t *settings);

/*
 * webconfig_start - Enable AP mode and start the HTTP config server.
 *
 * Creates a WiFi hotspot named WEBCONFIG_AP_SSID (open, no password),
 * starts a DHCP server, and listens on port 80 for HTTP requests.
 * The form is pre-populated from the current values in *settings.
 * When the user submits the form, *settings is updated and on_save
 * is called.
 *
 * Returns 0 on success, negative errno on failure.
 */
int webconfig_start(settings_t *settings, webconfig_callback_t on_save);

/*
 * webconfig_stop - Tear down AP mode and stop the HTTP server.
 */
void webconfig_stop(void);

#endif
