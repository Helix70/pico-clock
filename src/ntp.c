#include "ntp.h"
#include <zephyr/net/sntp.h>

#define EPOCH_YEAR 1970

static const uint16_t days_before_month[12] = {
    0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
};

static bool is_leap_year(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int days_in_year(int year)
{
    return is_leap_year(year) ? 366 : 365;
}

static int days_in_month(int year, int month)
{
    static const uint8_t mdays[12] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return mdays[month - 1];
}

static void epoch_to_time(int64_t epoch, ds1302_time_t *t)
{
    int32_t days = (int32_t)(epoch / 86400);
    int32_t rem  = (int32_t)(epoch % 86400);

    if (rem < 0) {
        rem += 86400;
        days--;
    }

    t->hours   = (uint8_t)(rem / 3600);
    t->minutes = (uint8_t)((rem % 3600) / 60);
    t->seconds = (uint8_t)(rem % 60);

    /* Jan 1 1970 was Thursday (4). DS1302: 1=Sunday. */
    int wday = (int)((days % 7 + 4) % 7) + 1;
    if (wday < 1) wday += 7;
    t->weekday = (uint8_t)wday;

    int year = EPOCH_YEAR;
    while (days >= days_in_year(year)) {
        days -= days_in_year(year);
        year++;
    }
    while (days < 0) {
        year--;
        days += days_in_year(year);
    }

    int month = 1;
    while (month < 12 && days >= days_in_month(year, month)) {
        days -= days_in_month(year, month);
        month++;
    }

    t->year  = (uint8_t)(year - 2000);
    t->month = (uint8_t)month;
    t->day   = (uint8_t)(days + 1);
}

int ntp_sync(const ntp_config_t *cfg, ds1302_time_t *time)
{
    struct sntp_time sntp_time;

    int ret = sntp_simple(cfg->server, cfg->timeout_ms, &sntp_time);
    if (ret < 0) {
        return ret;
    }

    int64_t local_epoch = (int64_t)sntp_time.seconds + cfg->utc_offset_sec;
    epoch_to_time(local_epoch, time);

    return 0;
}
