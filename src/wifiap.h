#ifndef WIFIAP_H
#define WIFIAP_H

#include <stdint.h>

/*
 * wifiap_start - Enable WiFi AP mode (open network, no password).
 *
 * @ssid:    Network name to broadcast.
 * @channel: WiFi channel (1-13).
 *
 * Returns 0 on success, negative errno on failure.
 */
int wifiap_start(const char *ssid, uint8_t channel);

/*
 * wifiap_stop - Disable WiFi AP mode.
 */
void wifiap_stop(void);

#endif
