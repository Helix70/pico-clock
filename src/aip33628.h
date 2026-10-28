#ifndef AIP33628_H
#define AIP33628_H

#include <stdint.h>
#include "hardware/pio.h"

/**
 * AiP33628 driver instance.
 *
 * Each instance owns one PIO state machine and drives one AiP33628 chip
 * over two GPIO pins (DIN and CLK).
 */
typedef struct {
    PIO pio;
    uint sm;
} aip33628_t;

/**
 * aip33628_init - Set up a PIO state machine to drive an AiP33628.
 *
 * @dev:      Driver instance to initialise.
 * @pio:      PIO block (pio0 or pio1).
 * @sm:       State machine index (0-3).
 * @pin_din:  GPIO number for the data line (DIN).
 * @pin_clk:  GPIO number for the clock line (CLK).
 * @freq_hz:  PIO clock frequency in Hz.
 *            Minimum ~30 kHz (display flicker).
 *            Maximum ~3 MHz (latch reliability).
 *
 * @return 0 on success, -1 if the PIO program could not be loaded.
 *
 * The function loads the PIO program, configures the state machine for
 * MSB-first output with 1-bit side-set, and starts the state machine.
 * After init the state machine idles waiting for data in the TX FIFO.
 *
 * Call this at least 200 us after chip power-on (AiP33628 ignores
 * data during its internal reset window).
 */
int aip33628_init(aip33628_t *dev, PIO pio, uint sm,
                  uint pin_din, uint pin_clk, float freq_hz);

/**
 * aip33628_write - Send a display frame to the AiP33628.
 *
 * @dev:      Initialised driver instance.
 * @segments: SEG outputs SS[15:0] — one bit per segment pin.
 * @commons:  COM anode drivers CS[7:0] — one bit per common pin.
 * @current:  LED drive current level 0-15 (0 = off, 15 = max).
 *
 * Packs the three fields into the AiP33628's 30-bit frame format,
 * reverses the bit order from the spec's LSB-first layout to the
 * MSB-first order required by the PIO program, and writes the
 * resulting 32-bit word to the TX FIFO.
 *
 * The current level is internally bit-reversed per the AiP33628
 * datasheet (IS[3:0] uses reverse-bit mapping on the wire).
 *
 * This call blocks if the TX FIFO is full.
 *
 * The segments parameter maps directly to the AiP33628's 16 SEG
 * output pins (SS[15:0]).  The commons parameter maps to the 8 COM
 * output pins (CS[7:0]).  How these pins are wired on a particular
 * PCB is board-specific — see hu058.h for the HU-058 clock board.
 */
void aip33628_write(aip33628_t *dev, uint16_t segments,
                    uint8_t commons, uint8_t current);

/**
 * aip33628_write_raw - Send a pre-packed 30-bit frame.
 *
 * @dev:    Initialised driver instance.
 * @frame:  30-bit frame in AiP33628 wire order (LSB-first layout,
 *          bits 31-30 ignored). The function handles the MSB-first
 *          reversal for the PIO.
 *
 * Use this when you have already packed the segment, common, and
 * current fields yourself. Blocks if the TX FIFO is full.
 */
void aip33628_write_raw(aip33628_t *dev, uint32_t frame);

#endif
