#ifndef SPEAKER_H
#define SPEAKER_H

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <stdint.h>

typedef struct {
    struct gpio_dt_spec gpio;
    struct k_timer timer;
} speaker_t;

/*
 * speaker_init - Configure the buzzer GPIO as open-drain and init the timer.
 *
 * Returns 0 on success, negative errno on failure.
 */
int speaker_init(speaker_t *spk);

/*
 * speaker_tone - Start playing a tone at the given frequency (Hz).
 *
 * Toggles the open-drain output at the requested frequency.
 * Frequencies below 20 Hz or above 20000 Hz are clamped.
 */
void speaker_tone(speaker_t *spk, uint32_t freq_hz);

/*
 * speaker_stop - Silence the speaker.
 */
void speaker_stop(speaker_t *spk);

/*
 * speaker_beep - Play a tone for a given duration, then silence.
 *
 * Blocks for duration_ms using k_msleep.
 */
void speaker_beep(speaker_t *spk, uint32_t freq_hz, uint32_t duration_ms);

#endif
