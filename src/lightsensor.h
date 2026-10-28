#ifndef LIGHTSENSOR_H
#define LIGHTSENSOR_H

#include <zephyr/drivers/adc.h>
#include <stdint.h>

/*
 * LDR light sensor configuration.
 *
 * The LDR sits in a voltage divider: pullup resistor from 3V3 to the
 * ADC pin, LDR from the ADC pin to GND.  Brighter light lowers the
 * LDR resistance, lowering the ADC voltage.
 *
 * On the HU-058 PCB, R1 (10k pullup) and R3 (GL5539 LDR) form this
 * divider.  The GL5539 ranges from ~30-90 kΩ in darkness to ~5-10 kΩ
 * at 10 lux.  The original circuit used 5V — rewire R1 to 3V3 for
 * the RP2040.
 */
typedef struct {
    struct adc_dt_spec adc;
    float r_pullup;
} lightsensor_config_t;

/*
 * lightsensor_init - Set up the ADC channel for light sensor reading.
 *
 * Returns 0 on success, negative errno on failure.
 */
int lightsensor_init(const lightsensor_config_t *cfg);

/*
 * lightsensor_read_raw - Read the raw ADC value.
 *
 * Higher values = darker (LDR resistance high, voltage high).
 * Lower values = brighter (LDR resistance low, voltage low).
 */
uint16_t lightsensor_read_raw(const lightsensor_config_t *cfg);

/*
 * lightsensor_read_brightness - Map the light level to a display
 * brightness level suitable for hu058_set_brightness (0-15).
 *
 * @min_level: Minimum brightness (returned in full darkness).
 * @max_level: Maximum brightness (returned in bright light).
 */
uint8_t lightsensor_read_brightness(const lightsensor_config_t *cfg,
                                    uint8_t min_level, uint8_t max_level);

#endif
