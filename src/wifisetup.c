#include "wifisetup.h"
#include "wifista.h"
#include "settings.h"

#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

LOG_MODULE_REGISTER(wifisetup, LOG_LEVEL_INF);

static const char *security_str(uint8_t security)
{
	switch (security) {
	case WIFI_SECURITY_TYPE_NONE:       return "Open";
	case WIFI_SECURITY_TYPE_PSK:        return "WPA2";
	case WIFI_SECURITY_TYPE_PSK_SHA256: return "WPA2-SHA256";
	case WIFI_SECURITY_TYPE_SAE:        return "WPA3";
	default:                            return "Unknown";
	}
}

static int read_line(const struct device *uart, char *buf, size_t maxlen,
		     bool echo)
{
	size_t pos = 0;
	unsigned char c;

	while (pos < maxlen - 1) {
		while (uart_poll_in(uart, &c) != 0) {
			k_msleep(10);
		}
		if (c == '\r' || c == '\n') {
			if (pos > 0) {
				printk("\n");
				break;
			}
			continue;
		}
		if (c == '\b' || c == 0x7f) {
			if (pos > 0) {
				pos--;
				printk("\b \b");
			}
			continue;
		}
		buf[pos++] = c;
		printk("%c", echo ? c : '*');
	}
	buf[pos] = '\0';
	return pos;
}

static void print_scan_results(const wifista_scan_results_t *results)
{
	printk("\n");
	for (int i = 0; i < results->count; i++) {
		const wifista_ap_t *ap = &results->aps[i];
		printk("  %2d. %-32s  %4d dBm  [%s]\n",
		       i + 1, ap->ssid, ap->rssi, security_str(ap->security));
	}
	printk("\n");
}

int wifisetup_run(settings_t *s)
{
	const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	wifista_scan_results_t results;
	char input[8];
	int ret;

	if (!device_is_ready(uart)) {
		LOG_ERR("Console device not ready");
		return -ENODEV;
	}

	while (true) {
		printk("Scanning WiFi networks...\n");
		ret = wifista_scan(&results);
		if (ret) {
			printk("Scan failed (%d). Retrying in 3s...\n", ret);
			k_sleep(K_SECONDS(3));
			continue;
		}

		if (results.count == 0) {
			printk("No networks found. Retrying in 3s...\n");
			k_sleep(K_SECONDS(3));
			continue;
		}

		print_scan_results(&results);
		printk("Enter number to connect (or 0 to rescan): ");
		read_line(uart, input, sizeof(input), true);
		int choice = atoi(input);

		if (choice == 0) {
			continue;
		}

		if (choice < 1 || choice > results.count) {
			printk("Invalid choice.\n");
			continue;
		}

		const wifista_ap_t *selected = &results.aps[choice - 1];
		char pw[SETTINGS_PASS_MAX + 1] = {0};

		if (selected->security != WIFI_SECURITY_TYPE_NONE) {
			printk("Password: ");
			read_line(uart, pw, sizeof(pw), false);
		}

		printk("Connecting to %s...\n", selected->ssid);
		ret = wifista_connect(selected->ssid, pw);
		if (ret) {
			printk("Connection failed (%d). Try again.\n", ret);
			continue;
		}

		strncpy(s->wifi_ssid, selected->ssid, SETTINGS_SSID_MAX);
		s->wifi_ssid[SETTINGS_SSID_MAX] = '\0';
		strncpy(s->wifi_pass, pw, SETTINGS_PASS_MAX);
		s->wifi_pass[SETTINGS_PASS_MAX] = '\0';

		settings_save(s);

		printk("Connected!\n");
		return 0;
	}
}
