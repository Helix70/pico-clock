#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#define SETTINGS_MAX_ALARMS     4
#define SETTINGS_SSID_MAX       32
#define SETTINGS_PASS_MAX       64
#define SETTINGS_NTP_SERVER_MAX 64

typedef struct {
    uint8_t hour;
    uint8_t minute;
    bool enabled;
} settings_alarm_t;

typedef struct {
    char wifi_ssid[SETTINGS_SSID_MAX + 1];
    char wifi_pass[SETTINGS_PASS_MAX + 1];

    bool fahrenheit;
    bool time_24h;
    bool hourly_beep;

    settings_alarm_t alarms[SETTINGS_MAX_ALARMS];

    char ntp_server[SETTINGS_NTP_SERVER_MAX + 1];
    int32_t utc_offset_sec;

    uint8_t brightness;
    bool led_r;
    bool led_g;
    bool led_b;
} settings_t;

int settings_load(settings_t *s);
int settings_save(const settings_t *s);

#endif
