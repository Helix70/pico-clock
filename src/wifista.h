#ifndef WIFISTA_H
#define WIFISTA_H

#include <zephyr/net/wifi_mgmt.h>
#include <stdint.h>

#define WIFISTA_MAX_RESULTS 20

typedef struct {
	char ssid[WIFI_SSID_MAX_LEN + 1];
	int8_t rssi;
	uint8_t channel;
	uint8_t security;
} wifista_ap_t;

typedef struct {
	wifista_ap_t aps[WIFISTA_MAX_RESULTS];
	uint8_t count;
} wifista_scan_results_t;

int wifista_scan(wifista_scan_results_t *results);
int wifista_connect(const char *ssid, const char *password);
int wifista_disconnect(void);

#endif
