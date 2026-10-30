#include "wifista.h"

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(wifista, LOG_LEVEL_INF);

static K_SEM_DEFINE(wifi_sem, 0, 1);

static wifista_scan_results_t *scan_out;
static int connect_status;

static struct net_mgmt_event_callback wifi_scan_cb;
static struct net_mgmt_event_callback wifi_connect_cb;

static void on_scan_result(struct net_mgmt_event_callback *cb,
			   uint64_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event != NET_EVENT_WIFI_SCAN_RESULT) {
		return;
	}

	const struct wifi_scan_result *r =
		(const struct wifi_scan_result *)cb->info;

	if (!scan_out || scan_out->count >= WIFISTA_MAX_RESULTS) {
		return;
	}

	/* Skip duplicate SSIDs, keep the one with stronger signal. */
	for (int i = 0; i < scan_out->count; i++) {
		if (strcmp(scan_out->aps[i].ssid, r->ssid) == 0) {
			if (r->rssi > scan_out->aps[i].rssi) {
				scan_out->aps[i].rssi = r->rssi;
				scan_out->aps[i].channel = r->channel;
				scan_out->aps[i].security = r->security;
			}
			return;
		}
	}

	wifista_ap_t *ap = &scan_out->aps[scan_out->count];
	size_t len = r->ssid_length;
	if (len > WIFI_SSID_MAX_LEN) {
		len = WIFI_SSID_MAX_LEN;
	}
	memcpy(ap->ssid, r->ssid, len);
	ap->ssid[len] = '\0';
	ap->rssi = r->rssi;
	ap->channel = r->channel;
	ap->security = r->security;
	scan_out->count++;
}

static void on_scan_done(struct net_mgmt_event_callback *cb,
			 uint64_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event == NET_EVENT_WIFI_SCAN_DONE) {
		k_sem_give(&wifi_sem);
	}
}

static void on_connect_result(struct net_mgmt_event_callback *cb,
			      uint64_t mgmt_event, struct net_if *iface)
{
	if (mgmt_event != NET_EVENT_WIFI_CONNECT_RESULT) {
		return;
	}

	const struct wifi_status *status =
		(const struct wifi_status *)cb->info;

	connect_status = status->status;
	k_sem_give(&wifi_sem);
}

int wifista_scan(wifista_scan_results_t *results)
{
	struct net_if *iface = net_if_get_default();
	if (!iface) {
		return -ENODEV;
	}

	memset(results, 0, sizeof(*results));
	scan_out = results;

	net_mgmt_init_event_callback(&wifi_scan_cb, on_scan_result,
				     NET_EVENT_WIFI_SCAN_RESULT);
	net_mgmt_add_event_callback(&wifi_scan_cb);

	struct net_mgmt_event_callback done_cb;
	net_mgmt_init_event_callback(&done_cb, on_scan_done,
				     NET_EVENT_WIFI_SCAN_DONE);
	net_mgmt_add_event_callback(&done_cb);

	k_sem_reset(&wifi_sem);

	int ret = net_mgmt(NET_REQUEST_WIFI_SCAN, iface, NULL, 0);
	if (ret) {
		LOG_ERR("Scan request failed: %d", ret);
		net_mgmt_del_event_callback(&wifi_scan_cb);
		net_mgmt_del_event_callback(&done_cb);
		scan_out = NULL;
		return ret;
	}

	ret = k_sem_take(&wifi_sem, K_SECONDS(15));
	net_mgmt_del_event_callback(&wifi_scan_cb);
	net_mgmt_del_event_callback(&done_cb);
	scan_out = NULL;

	if (ret) {
		LOG_ERR("Scan timed out");
		return -ETIMEDOUT;
	}

	LOG_INF("Scan complete: %d networks found", results->count);
	return 0;
}

int wifista_connect(const char *ssid, const char *password)
{
	struct net_if *iface = net_if_get_default();
	if (!iface) {
		return -ENODEV;
	}

	struct wifi_connect_req_params cnx = { 0 };
	cnx.ssid = (const uint8_t *)ssid;
	cnx.ssid_length = strlen(ssid);
	cnx.channel = WIFI_CHANNEL_ANY;
	cnx.band = WIFI_FREQ_BAND_2_4_GHZ;

	if (password && password[0] != '\0') {
		cnx.security = WIFI_SECURITY_TYPE_PSK;
		cnx.psk = (const uint8_t *)password;
		cnx.psk_length = strlen(password);
	} else {
		cnx.security = WIFI_SECURITY_TYPE_NONE;
	}

	net_mgmt_init_event_callback(&wifi_connect_cb, on_connect_result,
				     NET_EVENT_WIFI_CONNECT_RESULT);
	net_mgmt_add_event_callback(&wifi_connect_cb);

	k_sem_reset(&wifi_sem);
	connect_status = -1;

	int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface,
			   &cnx, sizeof(cnx));
	if (ret) {
		LOG_ERR("Connect request failed: %d", ret);
		net_mgmt_del_event_callback(&wifi_connect_cb);
		return ret;
	}

	ret = k_sem_take(&wifi_sem, K_SECONDS(30));
	net_mgmt_del_event_callback(&wifi_connect_cb);

	if (ret) {
		LOG_ERR("Connect timed out");
		return -ETIMEDOUT;
	}

	if (connect_status != 0) {
		LOG_ERR("Connect failed: status %d", connect_status);
		return -ECONNREFUSED;
	}

	LOG_INF("Connected to %s", ssid);
	return 0;
}

int wifista_disconnect(void)
{
	struct net_if *iface = net_if_get_default();
	if (!iface) {
		return -ENODEV;
	}

	return net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
}
