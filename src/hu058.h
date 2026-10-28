#ifndef HU058_H
#define HU058_H

#include "aip33628.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * HU-058 display abstraction.
 *
 * The HU-058 PCB mounts two AiP33628 drivers controlling four
 * seven-segment digits (hours tens/ones, minutes tens/ones) plus
 * five annunciators (AM, colon upper/lower, dash, degree).
 *
 *        aaaa    aaaa          aaaa  l aaaa
 *       f    b  f    b        f    b  f    b
 *       f h  b  f    b   j    f    b  f    b
 *        gggg    gggg   iiii   gggg    gggg
 *       e    c  e    c   k    e    c  e    c
 *       e    c  e    c        e    c  e    c
 *        dddd    dddd          dddd    dddd
 *
 *  a-g correspond to the standard seven-segment labeling.
 *  h = AM segment
 *  i = dash segment
 *  j = colon upper segment
 *  k = colon lower segment
 *  l = degree segment
 *
 * Each segment position is an RGB LED (3 SEG lines per LED).
 * The display is multiplexed at 1/4 duty using shorted COM pairs.
 *
 * Typical usage:
 *
 *     aip33628_t drv_h, drv_m;
 *     aip33628_init(&drv_h, ...);   // platform-specific pins/freq
 *     aip33628_init(&drv_m, ...);
 *
 *     hu058_display_t disp;
 *     hu058_init(&disp, &drv_h, &drv_m);
 *     hu058_set_time(&disp, 12, 34);
 *     hu058_set_brightness(&disp, 8);
 *     hu058_compile(&disp);
 *
 *     // in a timer ISR or tight loop (~1.6 kHz+ for flicker-free):
 *     hu058_scan_step(&disp);
 */

/*
 * Display wiring summary
 *
 * Each AiP33628 has 16 SEG pins and 8 COM pins.
 * RGB LEDs occupy SEG triples offset by one:
 *
 *   SEG0          — unused
 *   SEG1-3        — LED1 (R, G, B)   annunciator position
 *   SEG4-6        — LED2 (R, G, B)
 *   SEG7-9        — LED3 (R, G, B)
 *   SEG10-12      — LED4 (R, G, B)
 *   SEG13-15      — LED5 (R, G, B)
 *
 * COM outputs are shorted in pairs for 1/4 duty scanning:
 *
 *   Phase 0: COM0+COM1 (0x03)  tens digit, low segments
 *   Phase 1: COM2+COM3 (0x0C)  tens digit, high segments
 *   Phase 2: COM4+COM5 (0x30)  ones digit, low segments
 *   Phase 3: COM6+COM7 (0xC0)  ones digit, high segments
 *
 * Seven-segment mapping per COM phase:
 *
 *        aaaa
 *       f    b       COM-low (phase 0,2)    COM-high (phase 1,3)
 *       f    b       ─────────────────      ──────────────────
 *        gggg        LED3 → seg G           LED2 → seg D
 *       e    c       LED4 → seg F           LED3 → seg C
 *       e    c       LED5 → seg E           LED4 → seg B
 *        dddd                               LED5 → seg A
 *
 * LED1 carries annunciators instead of digit segments:
 *
 *   Driver 0 (hours):   block 0 LED1 = AM
 *                       block 1 LED1 = colon (upper=low, lower=high)
 *   Driver 1 (minutes): block 0 LED1 = dash
 *                       block 1 LED1 = degree
 */

/** Digit positions. */
enum hu058_pos {
  HU058_H_TENS = 0,
  HU058_H_ONES = 1,
  HU058_M_TENS = 2,
  HU058_M_ONES = 3,
  HU058_NUM_DIGITS = 4
};

/** 12-hour or 24-hour time format. */
enum hu058_time_format {
  HU058_24H = 0,
  HU058_12H = 1,
};

/** Annunciator identifiers. */
enum hu058_ann {
  HU058_ANN_AM = 0,
  HU058_ANN_COLON_UPPER = 1,
  HU058_ANN_COLON_LOWER = 2,
  HU058_ANN_DASH = 3,
  HU058_ANN_DEGREE = 4,
  HU058_NUM_ANN = 5
};

/** Seven-segment bitmask bits (bit 0 = segment a). */
enum hu058_seg {
  HU058_SEG_A = (1 << 0),
  HU058_SEG_B = (1 << 1),
  HU058_SEG_C = (1 << 2),
  HU058_SEG_D = (1 << 3),
  HU058_SEG_E = (1 << 4),
  HU058_SEG_F = (1 << 5),
  HU058_SEG_G = (1 << 6),
};

/** Per-channel on/off colour (intensity is controlled globally via current). */
typedef struct {
  bool r;
  bool g;
  bool b;
} hu058_color_t;

#define HU058_COLOR_RED ((hu058_color_t){true, false, false})
#define HU058_COLOR_GREEN ((hu058_color_t){false, true, false})
#define HU058_COLOR_BLUE ((hu058_color_t){false, false, true})
#define HU058_COLOR_WHITE ((hu058_color_t){true, true, true})
#define HU058_COLOR_OFF ((hu058_color_t){false, false, false})

/** Display state. */
typedef struct {
  aip33628_t *drv[2]; /* [0]=hours, [1]=minutes */

  uint8_t digits[HU058_NUM_DIGITS]; /* 7-seg bitmask per digit */
  hu058_color_t digit_color[HU058_NUM_DIGITS];

  bool ann[HU058_NUM_ANN]; /* annunciator on/off */
  hu058_color_t ann_color[HU058_NUM_ANN];

  enum hu058_time_format time_format;

  uint8_t current;    /* brightness 0-15 */
  uint8_t scan_phase; /* 0-3, advances each scan_step */

  uint16_t seg_buf[2][4]; /* compiled: [driver][phase] */
  uint8_t com_buf[4];     /* COM pair value per phase */
} hu058_display_t;

/**
 * hu058_init - Initialise the HU-058 display.
 *
 * @disp:  Display state to initialise.
 * @drv_h: Pre-initialised AiP33628 driver for the hours half.
 * @drv_m: Pre-initialised AiP33628 driver for the minutes half.
 *
 * Zeros all digits and annunciators, sets colour to white,
 * brightness to 8 (mid-range), and scan phase to 0.
 */
void hu058_init(hu058_display_t *disp, aip33628_t *drv_h, aip33628_t *drv_m);

/**
 * hu058_set_digit - Set a digit position to a decimal value 0-9.
 *
 * Values outside 0-9 blank the digit (all segments off).
 */
void hu058_set_digit(hu058_display_t *disp, enum hu058_pos pos, uint8_t value);

/**
 * hu058_set_segments - Set a digit position to an arbitrary segment mask.
 *
 * Use HU058_SEG_A … HU058_SEG_G constants or-ed together.
 */
void hu058_set_segments(hu058_display_t *disp, enum hu058_pos pos,
                        uint8_t segs);

/**
 * hu058_set_digit_color - Set the RGB colour for a digit position.
 */
void hu058_set_digit_color(hu058_display_t *disp, enum hu058_pos pos,
                           hu058_color_t color);

/**
 * hu058_set_ann - Enable or disable an annunciator.
 */
void hu058_set_ann(hu058_display_t *disp, enum hu058_ann ann, bool on);

/**
 * hu058_set_ann_color - Set the RGB colour for an annunciator.
 */
void hu058_set_ann_color(hu058_display_t *disp, enum hu058_ann ann,
                         hu058_color_t color);

/**
 * Perceptually linear brightness levels (gamma ≈ 2.2).
 *
 * Five steps that appear evenly spaced to the human eye,
 * mapped to AiP33628 current levels 0-15.
 */
#define HU058_BRIGHTNESS_1  0
#define HU058_BRIGHTNESS_2  1
#define HU058_BRIGHTNESS_3  3
#define HU058_BRIGHTNESS_4  8
#define HU058_BRIGHTNESS_5  15

/**
 * hu058_set_brightness - Set the global LED drive current level (0-15).
 */
void hu058_set_brightness(hu058_display_t *disp, uint8_t level);

/**
 * hu058_set_time_format - Select 12-hour or 24-hour display mode.
 *
 * In 12-hour mode, hu058_set_time automatically manages the AM annunciator.
 * In 24-hour mode (default), the AM annunciator is turned off by set_time.
 */
void hu058_set_time_format(hu058_display_t *disp, enum hu058_time_format fmt);

/**
 * hu058_set_time - Convenience: set all four digits from hours and minutes.
 *
 * @hours:   0-23 (or 1-12 for 12-hour mode; caller's choice).
 * @minutes: 0-59.
 *
 * Leading zero on the hours tens digit is blanked.
 */
void hu058_set_time(hu058_display_t *disp, uint8_t hours, uint8_t minutes);

/**
 * hu058_set_temperature - Display a temperature reading.
 *
 * @disp:       Display state.
 * @temp:       Temperature value, clamped to -99..999.
 * @fahrenheit: true for °F, false for °C.
 *
 * Uses the first three digit positions for the numeric value (with the
 * first position as a minus sign when negative) and the fourth digit
 * for "F" or "C".  Turns the degree annunciator on.
 * Leading zeros are blanked.
 */
void hu058_set_temperature(hu058_display_t *disp, int16_t temp, bool fahrenheit);

/**
 * hu058_compile - Recompute the scan buffer from the current logical state.
 *
 * Call after changing digits, colours, or annunciators, before the next
 * scan_step picks up the new state.
 */
void hu058_compile(hu058_display_t *disp);

/**
 * hu058_scan_step - Send one COM-pair frame to both drivers.
 *
 * Advances the internal scan phase (0 → 1 → 2 → 3 → 0 …).
 * Call at ~1.6 kHz or faster (4 phases × 415 Hz refresh minimum)
 * from a timer ISR or main loop. Four calls = one full display refresh.
 */
void hu058_scan_step(hu058_display_t *disp);

#endif
