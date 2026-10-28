#include "thermistor.h"
#include <zephyr/kernel.h>
#include <math.h>

#define KELVIN_OFFSET 273.15f

int thermistor_init(const thermistor_config_t *cfg)
{
    if (!adc_is_ready_dt(&cfg->adc)) {
        return -ENODEV;
    }
    return adc_channel_setup_dt(&cfg->adc);
}

static uint16_t read_adc(const thermistor_config_t *cfg)
{
    int16_t buf;
    struct adc_sequence seq = {
        .buffer = &buf,
        .buffer_size = sizeof(buf),
    };

    adc_sequence_init_dt(&cfg->adc, &seq);
    adc_read_dt(&cfg->adc, &seq);

    return (buf < 0) ? 0 : (uint16_t)buf;
}

float thermistor_read_c(const thermistor_config_t *cfg)
{
    uint16_t raw = read_adc(cfg);

    int resolution = BIT(cfg->adc.resolution) - 1;

    if (raw == 0)
        raw = 1;
    if (raw >= resolution)
        raw = resolution - 1;

    float r_ntc = cfg->r_pullup * (float)raw / ((float)resolution - (float)raw);

    float t_inv = 1.0f / (cfg->t_nominal + KELVIN_OFFSET)
                + (1.0f / cfg->beta) * logf(r_ntc / cfg->r_nominal);

    return (1.0f / t_inv) - KELVIN_OFFSET;
}

float thermistor_read_f(const thermistor_config_t *cfg)
{
    return thermistor_read_c(cfg) * 1.8f + 32.0f;
}
