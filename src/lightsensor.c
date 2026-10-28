#include "lightsensor.h"
#include <zephyr/kernel.h>

int lightsensor_init(const lightsensor_config_t *cfg)
{
    if (!adc_is_ready_dt(&cfg->adc)) {
        return -ENODEV;
    }
    return adc_channel_setup_dt(&cfg->adc);
}

static uint16_t read_adc(const lightsensor_config_t *cfg)
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

uint16_t lightsensor_read_raw(const lightsensor_config_t *cfg)
{
    return read_adc(cfg);
}

uint8_t lightsensor_read_brightness(const lightsensor_config_t *cfg,
                                    uint8_t min_level, uint8_t max_level)
{
    if (min_level > 15) min_level = 15;
    if (max_level > 15) max_level = 15;
    if (min_level > max_level) min_level = max_level;

    uint16_t raw = read_adc(cfg);
    int adc_max = BIT(cfg->adc.resolution) - 1;

    uint8_t range = max_level - min_level;
    uint8_t level = min_level + (uint8_t)((uint32_t)(adc_max - raw) * range / adc_max);

    return level;
}
