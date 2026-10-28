#include "wifiap.h"

#include <zephyr/device.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <string.h>

int wifiap_start(const char *ssid, uint8_t channel)
{
    struct net_if *iface = net_if_get_default();
    if (!iface) return -ENODEV;

    const struct device *dev = net_if_get_device(iface);
    if (!dev || !device_is_ready(dev)) {
        return -ENODEV;
    }

    struct wifi_connect_req_params params = {0};
    params.ssid = (const uint8_t *)ssid;
    params.ssid_length = strlen(ssid);
    params.channel = channel;
    params.security = WIFI_SECURITY_TYPE_NONE;

    return net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, iface,
                    &params, sizeof(params));
}

void wifiap_stop(void)
{
    struct net_if *iface = net_if_get_default();
    if (!iface) return;

    net_mgmt(NET_REQUEST_WIFI_AP_DISABLE, iface, NULL, 0);
}
