#include "aip33628.h"
#include "aip33628.pio.h"
#include "hardware/pio.h"

/**
 * Reverse the bottom @n bits of @v.
 * Bits above position n-1 in the result are zero.
 */
static uint32_t reverse_bits(uint32_t v, uint n)
{
    uint32_t r = 0;
    for (uint i = 0; i < n; i++) {
        r |= ((v >> i) & 1u) << (n - 1 - i);
    }
    return r;
}

/**
 * Reverse the 4-bit current field per the AiP33628 datasheet.
 * Logical level 1 → wire value 0x8, level 2 → 0x4, etc.
 */
static uint8_t reverse_current(uint8_t current)
{
    return (uint8_t)reverse_bits(current & 0x0Fu, 4);
}

int aip33628_init(aip33628_t *dev, PIO pio, uint sm,
                  uint pin_din, uint pin_clk, float freq_hz)
{
    if (!pio_can_add_program(pio, &aip33628_program))
        return -1;

    uint offset = pio_add_program(pio, &aip33628_program);
    aip33628_program_init(pio, sm, offset, pin_din, pin_clk, freq_hz);

    dev->pio = pio;
    dev->sm  = sm;

    return 0;
}

void aip33628_write(aip33628_t *dev, uint16_t segments,
                    uint8_t commons, uint8_t current)
{
    /*
     * Pack the 30-bit frame in the spec's LSB-first layout:
     *
     *   Bit  0-15  SS[15:0]  segment on/off
     *   Bit 16-23  CS[7:0]   common anode drivers
     *   Bit 24-27  IS[3:0]   current level (bit-reversed on wire)
     *   Bit 28-29  reserved  must be 0
     */
    uint32_t frame = (uint32_t)(segments & 0xFFFFu)
                   | ((uint32_t)(commons & 0xFFu) << 16)
                   | ((uint32_t)reverse_current(current) << 24);

    aip33628_write_raw(dev, frame);
}

void aip33628_write_raw(aip33628_t *dev, uint32_t frame)
{
    uint32_t word = reverse_bits(frame & 0x3FFFFFFFu, 30);
    if (!pio_sm_is_tx_fifo_full(dev->pio, dev->sm)) {
        pio_sm_put(dev->pio, dev->sm, word);
    }
}
