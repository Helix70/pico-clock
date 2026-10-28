#ifndef DS1302_H
#define DS1302_H

#include <zephyr/drivers/gpio.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    struct gpio_dt_spec ce;
    struct gpio_dt_spec sclk;
    struct gpio_dt_spec io;
} ds1302_t;

typedef struct {
    uint8_t seconds;   /* 0-59 */
    uint8_t minutes;   /* 0-59 */
    uint8_t hours;     /* 0-23 */
    uint8_t day;       /* 1-31 */
    uint8_t month;     /* 1-12 */
    uint8_t weekday;   /* 1-7, 1 = Sunday */
    uint8_t year;      /* 0-99 (offset from 2000) */
} ds1302_time_t;

/*
 * ds1302_init - Initialise GPIOs for the DS1302 3-wire interface.
 *
 * Configures CE and SCLK as outputs (driven low) and I/O as output (low).
 * Clears the clock-halt bit so the oscillator runs.
 *
 * Returns 0 on success, negative errno on GPIO failure.
 */
int ds1302_init(ds1302_t *dev);

/*
 * ds1302_get_time - Read all clock registers via burst read.
 */
void ds1302_get_time(ds1302_t *dev, ds1302_time_t *time);

/*
 * ds1302_set_time - Write all clock registers via burst write.
 *
 * Temporarily disables write-protect, writes seconds through year,
 * then re-enables write-protect.  Hours are stored in 24-hour mode.
 */
void ds1302_set_time(ds1302_t *dev, const ds1302_time_t *time);

/*
 * ds1302_write_protect - Enable or disable the write-protect bit.
 */
void ds1302_write_protect(ds1302_t *dev, bool enable);

/*
 * ds1302_read_ram - Read one byte from the 31-byte battery-backed RAM.
 *
 * @addr: 0-30.
 */
uint8_t ds1302_read_ram(ds1302_t *dev, uint8_t addr);

/*
 * ds1302_write_ram - Write one byte to the 31-byte battery-backed RAM.
 *
 * @addr: 0-30.
 */
void ds1302_write_ram(ds1302_t *dev, uint8_t addr, uint8_t data);

#endif
