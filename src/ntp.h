#ifndef NTP_H
#define NTP_H

#include "ds1302.h"
#include <stdint.h>

typedef struct {
    const char *server;
    int32_t utc_offset_sec;
    uint32_t timeout_ms;
} ntp_config_t;

/*
 * ntp_sync - Retrieve current time from an NTP server.
 *
 * Sends an SNTP request to cfg->server, applies the UTC offset,
 * and fills the ds1302_time_t struct with local time.
 *
 * The caller must ensure network connectivity before calling.
 *
 * Returns 0 on success, negative errno on failure.
 */
int ntp_sync(const ntp_config_t *cfg, ds1302_time_t *time);

#endif
