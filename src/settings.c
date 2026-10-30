#include "settings.h"

#include <zephyr/device.h>
#include <zephyr/kvss/nvs.h>
#include <zephyr/storage/flash_map.h>
#include <string.h>

#define NVS_ID_MAGIC      0x0000
#define NVS_ID_SSID       0x0001
#define NVS_ID_PASS       0x0002
#define NVS_ID_PREFS      0x0003
#define NVS_ID_ALARMS     0x0004
#define NVS_ID_NTP_SERVER 0x0005
#define NVS_ID_UTC_OFFSET 0x0006
#define NVS_ID_DISPLAY_HW 0x0007
#define NVS_ID_TEMP_OFFSET 0x0008

#define SETTINGS_MAGIC    0x01

static struct nvs_fs fs;
static bool nvs_ready;

static void set_defaults(settings_t *s)
{
    memset(s, 0, sizeof(*s));
    s->time_24h = true;
    strncpy(s->ntp_server, "pool.ntp.org", SETTINGS_NTP_SERVER_MAX);
    s->brightness = 3;
    s->led_r = true;
    s->led_g = true;
    s->led_b = true;
}

static int nvs_init(void)
{
    if (nvs_ready) return 0;

    fs.flash_device = PARTITION_DEVICE(storage_partition);
    if (!device_is_ready(fs.flash_device)) return -ENODEV;

    fs.offset = PARTITION_OFFSET(storage_partition);
    fs.sector_size = 4096;
    fs.sector_count = PARTITION_SIZE(storage_partition) / fs.sector_size;

    int ret = nvs_mount(&fs);
    if (ret < 0) return ret;

    nvs_ready = true;
    return 0;
}

int settings_load(settings_t *s)
{
    int ret = nvs_init();
    if (ret < 0) {
        set_defaults(s);
        return ret;
    }

    uint8_t magic;
    ret = nvs_read(&fs, NVS_ID_MAGIC, &magic, sizeof(magic));
    if (ret <= 0 || magic != SETTINGS_MAGIC) {
        set_defaults(s);
        settings_save(s);
        nvs_write(&fs, NVS_ID_MAGIC, &(uint8_t){SETTINGS_MAGIC}, 1);
        return 0;
    }

    set_defaults(s);

    nvs_read(&fs, NVS_ID_SSID, s->wifi_ssid, sizeof(s->wifi_ssid));
    nvs_read(&fs, NVS_ID_PASS, s->wifi_pass, sizeof(s->wifi_pass));

    uint8_t prefs;
    if (nvs_read(&fs, NVS_ID_PREFS, &prefs, sizeof(prefs)) > 0) {
        s->fahrenheit  = (prefs & 0x01) != 0;
        s->time_24h    = (prefs & 0x02) != 0;
        s->hourly_beep = (prefs & 0x04) != 0;
    }

    uint8_t alarm_buf[SETTINGS_MAX_ALARMS * 3];
    if (nvs_read(&fs, NVS_ID_ALARMS, alarm_buf, sizeof(alarm_buf)) > 0) {
        for (int i = 0; i < SETTINGS_MAX_ALARMS; i++) {
            s->alarms[i].hour    = alarm_buf[i * 3];
            s->alarms[i].minute  = alarm_buf[i * 3 + 1];
            s->alarms[i].enabled = alarm_buf[i * 3 + 2] != 0;
        }
    }

    nvs_read(&fs, NVS_ID_NTP_SERVER, s->ntp_server, sizeof(s->ntp_server));
    nvs_read(&fs, NVS_ID_UTC_OFFSET, &s->utc_offset_sec,
             sizeof(s->utc_offset_sec));

    uint8_t disp_hw;
    if (nvs_read(&fs, NVS_ID_DISPLAY_HW, &disp_hw, sizeof(disp_hw)) > 0) {
        s->led_r      = (disp_hw & 0x01) != 0;
        s->led_g      = (disp_hw & 0x02) != 0;
        s->led_b      = (disp_hw & 0x04) != 0;
        s->brightness = (disp_hw >> 4) & 0x0F;
    }

    nvs_read(&fs, NVS_ID_TEMP_OFFSET, &s->temp_offset_tenths,
             sizeof(s->temp_offset_tenths));

    return 0;
}

int settings_save(const settings_t *s)
{
    int ret = nvs_init();
    if (ret < 0) return ret;

    nvs_write(&fs, NVS_ID_SSID, s->wifi_ssid, sizeof(s->wifi_ssid));
    nvs_write(&fs, NVS_ID_PASS, s->wifi_pass, sizeof(s->wifi_pass));

    uint8_t prefs = 0;
    if (s->fahrenheit)  prefs |= 0x01;
    if (s->time_24h)    prefs |= 0x02;
    if (s->hourly_beep) prefs |= 0x04;
    nvs_write(&fs, NVS_ID_PREFS, &prefs, sizeof(prefs));

    uint8_t alarm_buf[SETTINGS_MAX_ALARMS * 3];
    for (int i = 0; i < SETTINGS_MAX_ALARMS; i++) {
        alarm_buf[i * 3]     = s->alarms[i].hour;
        alarm_buf[i * 3 + 1] = s->alarms[i].minute;
        alarm_buf[i * 3 + 2] = s->alarms[i].enabled ? 1 : 0;
    }
    nvs_write(&fs, NVS_ID_ALARMS, alarm_buf, sizeof(alarm_buf));

    nvs_write(&fs, NVS_ID_NTP_SERVER, s->ntp_server, sizeof(s->ntp_server));
    nvs_write(&fs, NVS_ID_UTC_OFFSET, &s->utc_offset_sec,
              sizeof(s->utc_offset_sec));

    uint8_t disp_hw = 0;
    if (s->led_r) disp_hw |= 0x01;
    if (s->led_g) disp_hw |= 0x02;
    if (s->led_b) disp_hw |= 0x04;
    disp_hw |= (s->brightness & 0x0F) << 4;
    nvs_write(&fs, NVS_ID_DISPLAY_HW, &disp_hw, sizeof(disp_hw));

    nvs_write(&fs, NVS_ID_TEMP_OFFSET, &s->temp_offset_tenths,
              sizeof(s->temp_offset_tenths));

    uint8_t magic = SETTINGS_MAGIC;
    nvs_write(&fs, NVS_ID_MAGIC, &magic, sizeof(magic));

    return 0;
}
