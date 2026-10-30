#include "buttons.h"

#include <zephyr/kernel.h>

#define POLL_INTERVAL_MS  20
#define DEBOUNCE_SAMPLES  3
#define BOTH_HELD_TICKS   (2000 / POLL_INTERVAL_MS)

static const buttons_config_t *hw;
static button_callback_t cb;

static uint8_t raw1_hist;
static uint8_t raw2_hist;
static bool stable1;
static bool stable2;
static uint32_t both_held_ticks;
static bool both_held_fired;
static uint32_t btn1_held_ticks;
static bool btn1_held_fired;

static struct k_timer poll_timer;

static bool debounce(uint8_t *hist, bool sample)
{
    *hist = ((*hist << 1) | (sample ? 1 : 0)) & ((1 << DEBOUNCE_SAMPLES) - 1);
    uint8_t mask = (1 << DEBOUNCE_SAMPLES) - 1;
    if (*hist == mask) return true;
    if (*hist == 0)    return false;
    return sample;
}

static void poll_handler(struct k_timer *timer)
{
    ARG_UNUSED(timer);

    bool s1 = gpio_pin_get_dt(&hw->btn1) != 0;
    bool s2 = gpio_pin_get_dt(&hw->btn2) != 0;

    bool new1 = debounce(&raw1_hist, s1);
    bool new2 = debounce(&raw2_hist, s2);

    if (new1 != stable1) {
        stable1 = new1;
        if (cb) cb(new1 ? BUTTON_1_PRESS : BUTTON_1_RELEASE);
    }

    if (new2 != stable2) {
        stable2 = new2;
        if (cb) cb(new2 ? BUTTON_2_PRESS : BUTTON_2_RELEASE);
    }

    if (stable1 && stable2) {
        both_held_ticks++;
        if (both_held_ticks >= BOTH_HELD_TICKS && !both_held_fired) {
            both_held_fired = true;
            if (cb) cb(BUTTON_BOTH_HELD);
        }
        btn1_held_ticks = 0;
        btn1_held_fired = false;
    } else {
        both_held_ticks = 0;
        both_held_fired = false;
        if (stable1 && !stable2) {
            btn1_held_ticks++;
            if (btn1_held_ticks >= BOTH_HELD_TICKS && !btn1_held_fired) {
                btn1_held_fired = true;
                if (cb) cb(BUTTON_1_HELD);
            }
        } else {
            btn1_held_ticks = 0;
            btn1_held_fired = false;
        }
    }
}

int buttons_init(const buttons_config_t *cfg, button_callback_t callback)
{
    hw = cfg;
    cb = callback;

    if (!gpio_is_ready_dt(&hw->btn1) || !gpio_is_ready_dt(&hw->btn2)) {
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&hw->btn1, GPIO_INPUT);
    if (ret < 0) return ret;

    ret = gpio_pin_configure_dt(&hw->btn2, GPIO_INPUT);
    if (ret < 0) return ret;

    raw1_hist = 0;
    raw2_hist = 0;
    stable1 = false;
    stable2 = false;
    both_held_ticks = 0;
    both_held_fired = false;
    btn1_held_ticks = 0;
    btn1_held_fired = false;

    k_timer_init(&poll_timer, poll_handler, NULL);
    k_timer_start(&poll_timer, K_MSEC(POLL_INTERVAL_MS),
                  K_MSEC(POLL_INTERVAL_MS));

    return 0;
}

void buttons_stop(void)
{
    k_timer_stop(&poll_timer);
    cb = NULL;
}
