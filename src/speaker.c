#include "speaker.h"

#define FREQ_MIN  20
#define FREQ_MAX  20000

static void toggle_handler(struct k_timer *t)
{
    speaker_t *spk = CONTAINER_OF(t, speaker_t, timer);
    gpio_pin_toggle_dt(&spk->gpio);
}

int speaker_init(speaker_t *spk)
{
    if (!gpio_is_ready_dt(&spk->gpio)) {
        return -ENODEV;
    }

    int ret = gpio_pin_configure_dt(&spk->gpio,
                                    GPIO_OUTPUT_INACTIVE | GPIO_OPEN_DRAIN);
    if (ret < 0) {
        return ret;
    }

    k_timer_init(&spk->timer, toggle_handler, NULL);
    return 0;
}

void speaker_tone(speaker_t *spk, uint32_t freq_hz)
{
    if (freq_hz < FREQ_MIN) freq_hz = FREQ_MIN;
    if (freq_hz > FREQ_MAX) freq_hz = FREQ_MAX;

    uint32_t half_period_us = 500000 / freq_hz;
    k_timer_start(&spk->timer, K_USEC(half_period_us),
                  K_USEC(half_period_us));
}

void speaker_stop(speaker_t *spk)
{
    k_timer_stop(&spk->timer);
    gpio_pin_set_dt(&spk->gpio, 0);
}

void speaker_beep(speaker_t *spk, uint32_t freq_hz, uint32_t duration_ms)
{
    speaker_tone(spk, freq_hz);
    k_msleep(duration_ms);
    speaker_stop(spk);
}
