#ifndef BUTTONS_H
#define BUTTONS_H

#include <zephyr/drivers/gpio.h>

typedef enum {
    BUTTON_1_PRESS,
    BUTTON_1_RELEASE,
    BUTTON_2_PRESS,
    BUTTON_2_RELEASE,
    BUTTON_BOTH_HELD,
} button_event_t;

typedef void (*button_callback_t)(button_event_t event);

typedef struct {
    struct gpio_dt_spec btn1;
    struct gpio_dt_spec btn2;
} buttons_config_t;

int buttons_init(const buttons_config_t *cfg, button_callback_t callback);
void buttons_stop(void);

#endif
