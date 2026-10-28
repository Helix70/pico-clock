/*
 * Auto-generated from aip33628.pio -- hand-assembled for use without pioasm.
 */

#pragma once

#if !PICO_NO_HARDWARE
#include "hardware/pio.h"
#endif

#define aip33628_wrap_target 0
#define aip33628_wrap 12

static const uint16_t aip33628_program_instructions[] = {
    0x8860, /*  0: pull   block           side 0 [8] */
    0xe101, /*  1: set    pins, 1         side 0 [1] */
    0xe000, /*  2: set    pins, 0         side 0     */
    0x6042, /*  3: out    y, 2            side 0     */
    0xe05d, /*  4: set    y, 29           side 0     */
    0xa042, /*  5: nop                    side 0     */
    0x6001, /*  6: out    pins, 1         side 0     */
    0xb042, /*  7: nop                    side 1     */
    0x1085, /*  8: jmp    y--, 5          side 1     */
    0xf400, /*  9: set    pins, 0         side 1 [4] */
    0xf101, /* 10: set    pins, 1         side 1 [1] */
    0xf000, /* 11: set    pins, 0         side 1     */
    0xa042, /* 12: nop                    side 0     */
};

#if !PICO_NO_HARDWARE
static const struct pio_program aip33628_program = {
    .instructions = aip33628_program_instructions,
    .length = 13,
    .origin = -1,
};

static inline pio_sm_config aip33628_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + aip33628_wrap_target,
                       offset + aip33628_wrap);
    sm_config_set_sideset(&c, 1, false, false);
    return c;
}

#include "hardware/clocks.h"

static inline void aip33628_program_init(PIO pio, uint sm, uint offset,
                                         uint pin_din, uint pin_clk,
                                         float freq_hz) {
    pio_sm_config c = aip33628_program_get_default_config(offset);

    sm_config_set_set_pins(&c, pin_din, 1);
    sm_config_set_out_pins(&c, pin_din, 1);
    pio_gpio_init(pio, pin_din);
    pio_sm_set_consecutive_pindirs(pio, sm, pin_din, 1, true);

    sm_config_set_sideset_pins(&c, pin_clk);
    pio_gpio_init(pio, pin_clk);
    pio_sm_set_consecutive_pindirs(pio, sm, pin_clk, 1, true);

    sm_config_set_out_shift(&c, false, false, 32);

    float div = (float)clock_get_hz(clk_sys) / freq_hz;
    sm_config_set_clkdiv(&c, div);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}
#endif
