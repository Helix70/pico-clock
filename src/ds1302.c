#include "ds1302.h"
#include <zephyr/kernel.h>

#define CMD_READ      0x01
#define CMD_WRITE     0x00
#define CMD_CLOCK     0x00
#define CMD_RAM       0x40

#define REG_SECONDS   0
#define REG_MINUTES   1
#define REG_HOURS     2
#define REG_DAY       3
#define REG_MONTH     4
#define REG_WEEKDAY   5
#define REG_YEAR      6
#define REG_WP        7

#define CMD_BURST_READ   0xBF
#define CMD_BURST_WRITE  0xBE
#define CMD_WP_WRITE     0x8E

#define DELAY_US 2

static uint8_t bcd_to_bin(uint8_t bcd)
{
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

static uint8_t bin_to_bcd(uint8_t bin)
{
    return ((bin / 10) << 4) | (bin % 10);
}

static uint8_t make_cmd(uint8_t reg, bool ram, bool read)
{
    return 0x80 | (ram ? CMD_RAM : CMD_CLOCK) | (reg << 1) | (read ? CMD_READ : CMD_WRITE);
}

static void io_output(ds1302_t *dev)
{
    gpio_pin_configure_dt(&dev->io, GPIO_OUTPUT_LOW);
}

static void io_input(ds1302_t *dev)
{
    gpio_pin_configure_dt(&dev->io, GPIO_INPUT);
}

static void write_byte(ds1302_t *dev, uint8_t val)
{
    io_output(dev);
    for (int i = 0; i < 8; i++) {
        gpio_pin_set_dt(&dev->io, (val >> i) & 1);
        k_busy_wait(DELAY_US);
        gpio_pin_set_dt(&dev->sclk, 1);
        k_busy_wait(DELAY_US);
        gpio_pin_set_dt(&dev->sclk, 0);
    }
}

static uint8_t read_byte(ds1302_t *dev)
{
    io_input(dev);
    uint8_t val = 0;
    for (int i = 0; i < 8; i++) {
        val |= (gpio_pin_get_dt(&dev->io) ? 1 : 0) << i;
        k_busy_wait(DELAY_US);
        gpio_pin_set_dt(&dev->sclk, 1);
        k_busy_wait(DELAY_US);
        gpio_pin_set_dt(&dev->sclk, 0);
    }
    return val;
}

static void begin(ds1302_t *dev)
{
    gpio_pin_set_dt(&dev->sclk, 0);
    gpio_pin_set_dt(&dev->ce, 1);
    k_busy_wait(DELAY_US);
}

static void end(ds1302_t *dev)
{
    gpio_pin_set_dt(&dev->ce, 0);
    k_busy_wait(DELAY_US);
    io_output(dev);
    gpio_pin_set_dt(&dev->io, 0);
}

static void write_reg(ds1302_t *dev, uint8_t reg, bool ram, uint8_t data)
{
    begin(dev);
    write_byte(dev, make_cmd(reg, ram, false));
    write_byte(dev, data);
    end(dev);
}

static uint8_t read_reg(ds1302_t *dev, uint8_t reg, bool ram)
{
    begin(dev);
    write_byte(dev, make_cmd(reg, ram, true));
    uint8_t val = read_byte(dev);
    end(dev);
    return val;
}

int ds1302_init(ds1302_t *dev)
{
    int ret;

    ret = gpio_pin_configure_dt(&dev->ce, GPIO_OUTPUT_LOW);
    if (ret < 0) return ret;

    ret = gpio_pin_configure_dt(&dev->sclk, GPIO_OUTPUT_LOW);
    if (ret < 0) return ret;

    ret = gpio_pin_configure_dt(&dev->io, GPIO_OUTPUT_LOW);
    if (ret < 0) return ret;

    ds1302_write_protect(dev, false);

    uint8_t sec = read_reg(dev, REG_SECONDS, false);
    if (sec & 0x80) {
        write_reg(dev, REG_SECONDS, false, sec & 0x7F);
    }

    ds1302_write_protect(dev, true);

    return 0;
}

void ds1302_get_time(ds1302_t *dev, ds1302_time_t *time)
{
    begin(dev);
    write_byte(dev, CMD_BURST_READ);

    uint8_t sec  = read_byte(dev);
    uint8_t min  = read_byte(dev);
    uint8_t hr   = read_byte(dev);
    uint8_t day  = read_byte(dev);
    uint8_t mon  = read_byte(dev);
    uint8_t wday = read_byte(dev);
    uint8_t yr   = read_byte(dev);

    end(dev);

    time->seconds = bcd_to_bin(sec & 0x7F);
    time->minutes = bcd_to_bin(min & 0x7F);
    time->hours   = bcd_to_bin(hr & 0x3F);
    time->day     = bcd_to_bin(day & 0x3F);
    time->month   = bcd_to_bin(mon & 0x1F);
    time->weekday = bcd_to_bin(wday & 0x07);
    time->year    = bcd_to_bin(yr);
}

void ds1302_set_time(ds1302_t *dev, const ds1302_time_t *time)
{
    ds1302_write_protect(dev, false);

    begin(dev);
    write_byte(dev, CMD_BURST_WRITE);

    write_byte(dev, bin_to_bcd(time->seconds) & 0x7F);
    write_byte(dev, bin_to_bcd(time->minutes));
    write_byte(dev, bin_to_bcd(time->hours) & 0x3F);
    write_byte(dev, bin_to_bcd(time->day));
    write_byte(dev, bin_to_bcd(time->month));
    write_byte(dev, bin_to_bcd(time->weekday));
    write_byte(dev, bin_to_bcd(time->year));
    write_byte(dev, 0x80);

    end(dev);
}

void ds1302_write_protect(ds1302_t *dev, bool enable)
{
    begin(dev);
    write_byte(dev, CMD_WP_WRITE);
    write_byte(dev, enable ? 0x80 : 0x00);
    end(dev);
}

uint8_t ds1302_read_ram(ds1302_t *dev, uint8_t addr)
{
    if (addr > 30) addr = 30;
    return read_reg(dev, addr, true);
}

void ds1302_write_ram(ds1302_t *dev, uint8_t addr, uint8_t data)
{
    if (addr > 30) addr = 30;
    ds1302_write_protect(dev, false);
    write_reg(dev, addr, true, data);
    ds1302_write_protect(dev, true);
}
