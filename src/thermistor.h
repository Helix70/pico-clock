#ifndef THERMISTOR_H
#define THERMISTOR_H

#include <zephyr/drivers/adc.h>
#include <stdint.h>

/*
 * NTC thermistor configuration.
 *
 * The thermistor sits in a voltage divider: pullup resistor from 3V3
 * to the ADC pin, NTC from the ADC pin to GND.
 *
 * On the HU-058 PCB, R2 (10k pullup) and R4 (10k NTC) form this
 * divider.  The original circuit used 5V — rewire R2 to 3V3 when
 * driving with an RP2040.
 */
typedef struct {
    struct adc_dt_spec adc;
    float r_pullup;
    float r_nominal;
    float t_nominal;
    float beta;
} thermistor_config_t;

/*
 * thermistor_init - Set up the ADC channel for thermistor reading.
 *
 * Returns 0 on success, negative errno on failure.
 */
int thermistor_init(const thermistor_config_t *cfg);

/*
 * thermistor_read_c - Read the thermistor temperature in degrees C.
 *
 * Uses the beta (simplified Steinhart-Hart) equation.
 */
float thermistor_read_c(const thermistor_config_t *cfg);

/*
 * thermistor_read_f - Read the thermistor temperature in degrees F.
 */
float thermistor_read_f(const thermistor_config_t *cfg);

#endif
