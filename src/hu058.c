#include "hu058.h"
#include <string.h>

/*
 * Seven-segment font: bit 0 = a (top), 1 = b (top-right), 2 = c (bottom-right),
 * 3 = d (bottom), 4 = e (bottom-left), 5 = f (top-left), 6 = g (middle).
 */
static const uint8_t FONT[10] = {
    /* 0 */ HU058_SEG_A | HU058_SEG_B | HU058_SEG_C | HU058_SEG_D |
        HU058_SEG_E | HU058_SEG_F,
    /* 1 */ HU058_SEG_B | HU058_SEG_C,
    /* 2 */ HU058_SEG_A | HU058_SEG_B | HU058_SEG_D | HU058_SEG_E | HU058_SEG_G,
    /* 3 */ HU058_SEG_A | HU058_SEG_B | HU058_SEG_C | HU058_SEG_D | HU058_SEG_G,
    /* 4 */ HU058_SEG_B | HU058_SEG_C | HU058_SEG_F | HU058_SEG_G,
    /* 5 */ HU058_SEG_A | HU058_SEG_C | HU058_SEG_D | HU058_SEG_F | HU058_SEG_G,
    /* 6 */ HU058_SEG_A | HU058_SEG_C | HU058_SEG_D | HU058_SEG_E |
        HU058_SEG_F | HU058_SEG_G,
    /* 7 */ HU058_SEG_A | HU058_SEG_B | HU058_SEG_C,
    /* 8 */ HU058_SEG_A | HU058_SEG_B | HU058_SEG_C | HU058_SEG_D |
        HU058_SEG_E | HU058_SEG_F | HU058_SEG_G,
    /* 9 */ HU058_SEG_A | HU058_SEG_B | HU058_SEG_C | HU058_SEG_D |
        HU058_SEG_F | HU058_SEG_G,
};

#define SEG_LETTER_C (HU058_SEG_A | HU058_SEG_D | HU058_SEG_E | HU058_SEG_F)
#define SEG_LETTER_F (HU058_SEG_A | HU058_SEG_E | HU058_SEG_F | HU058_SEG_G)
#define SEG_MINUS (HU058_SEG_G)
#define SEG_LETTER_B                                                           \
  (HU058_SEG_C | HU058_SEG_D | HU058_SEG_E | HU058_SEG_F | HU058_SEG_G)
#define SEG_LETTER_O (HU058_SEG_C | HU058_SEG_D | HU058_SEG_E | HU058_SEG_G)
#define SEG_LETTER_T (HU058_SEG_D | HU058_SEG_E | HU058_SEG_F | HU058_SEG_G)

/*
 * COM pair values for the four scan phases.
 * Tens digits sit on COM4-7 (0x30 low, 0xC0 high),
 * ones digits on COM0-3 (0x03 low, 0x0C high).
 */
static const uint8_t COM_PAIRS[4] = {0x30, 0xC0, 0x03, 0x0C};

/*
 * SEG bit positions for each LED's R/G/B channels.
 * LED1 = SEG1-3, LED2 = SEG4-6, … LED5 = SEG13-15.
 * Within a triple the order is blue, green, red.
 */
#define LED_B(n) (1u << (((n) - 1) * 3 + 1))
#define LED_G(n) (1u << (((n) - 1) * 3 + 2))
#define LED_R(n) (1u << (((n) - 1) * 3 + 3))

static uint16_t led_bits(int led, hu058_color_t c) {
  uint16_t bits = 0;
  if (c.r)
    bits |= LED_R(led);
  if (c.g)
    bits |= LED_G(led);
  if (c.b)
    bits |= LED_B(led);
  return bits;
}

/*
 * Annunciator-to-driver/phase mapping.
 *
 * Each annunciator occupies the LED1 position of a specific
 * driver and COM phase:
 *
 *   AM           → driver 0, phase 1 (hour tens, COM-high)
 *   COLON_UPPER  → driver 0, phase 3 (hour ones, COM-high)
 *   COLON_LOWER  → driver 0, phase 2 (hour ones, COM-low)
 *   DASH         → driver 1, phase 1 (min tens, COM-high)
 *   DEGREE       → driver 1, phase 3 (min ones, COM-high)
 */
static const struct {
  uint8_t driver;
  uint8_t phase;
} ANN_MAP[HU058_NUM_ANN] = {
    [HU058_ANN_AM] = {0, 1},          [HU058_ANN_COLON_UPPER] = {0, 3},
    [HU058_ANN_COLON_LOWER] = {0, 2}, [HU058_ANN_DASH] = {1, 1},
    [HU058_ANN_DEGREE] = {1, 3},
};

/*
 * Digit-to-driver mapping.
 * H_TENS and H_ONES are on driver 0 (hours).
 * M_TENS and M_ONES are on driver 1 (minutes).
 */
static const uint8_t DIGIT_DRIVER[HU058_NUM_DIGITS] = {0, 0, 1, 1};

/*
 * Digit-to-block mapping within a driver.
 * Tens digits use block 0 (COM phases 0-1, 0x30/0xC0).
 * Ones digits use block 1 (COM phases 2-3, 0x03/0x0C).
 */
static const uint8_t DIGIT_BLOCK[HU058_NUM_DIGITS] = {0, 1, 0, 1};

void hu058_init(hu058_display_t *disp, aip33628_t *drv_h, aip33628_t *drv_m) {
  memset(disp, 0, sizeof(*disp));

  disp->drv[0] = drv_h;
  disp->drv[1] = drv_m;

  for (int i = 0; i < HU058_NUM_DIGITS; i++)
    disp->digit_color[i] = HU058_COLOR_WHITE;

  for (int i = 0; i < HU058_NUM_ANN; i++)
    disp->ann_color[i] = HU058_COLOR_WHITE;

  disp->current = 8;
  disp->scan_phase = 0;

  for (int i = 0; i < 4; i++)
    disp->com_buf[i] = COM_PAIRS[i];
}

void hu058_set_digit(hu058_display_t *disp, enum hu058_pos pos, uint8_t value) {
  disp->digits[pos] = (value < 10) ? FONT[value] : 0;
}

void hu058_set_segments(hu058_display_t *disp, enum hu058_pos pos,
                        uint8_t segs) {
  disp->digits[pos] = segs & 0x7F;
}

void hu058_set_digit_color(hu058_display_t *disp, enum hu058_pos pos,
                           hu058_color_t color) {
  disp->digit_color[pos] = color;
}

void hu058_set_ann(hu058_display_t *disp, enum hu058_ann ann, bool on) {
  disp->ann[ann] = on;
}

void hu058_set_ann_color(hu058_display_t *disp, enum hu058_ann ann,
                         hu058_color_t color) {
  disp->ann_color[ann] = color;
}

void hu058_set_brightness(hu058_display_t *disp, uint8_t level) {
  disp->current = (level > 15) ? 15 : level;
}

void hu058_set_time_format(hu058_display_t *disp, enum hu058_time_format fmt) {
  disp->time_format = fmt;
}

void hu058_set_time(hu058_display_t *disp, uint8_t hours, uint8_t minutes) {
  bool am = true;

  if (disp->time_format == HU058_12H) {
    if (hours == 0) {
      hours = 12;
      am = true;
    } else if (hours < 12) {
      am = true;
    } else if (hours == 12) {
      am = false;
    } else {
      hours -= 12;
      am = false;
    }
    disp->ann[HU058_ANN_AM] = am;
  } else {
    disp->ann[HU058_ANN_AM] = false;
  }

  uint8_t h_tens = hours / 10;
  uint8_t h_ones = hours % 10;
  uint8_t m_tens = minutes / 10;
  uint8_t m_ones = minutes % 10;

  disp->digits[HU058_H_TENS] = (h_tens > 0) ? FONT[h_tens] : 0;
  disp->digits[HU058_H_ONES] = FONT[h_ones];
  disp->digits[HU058_M_TENS] = FONT[m_tens];
  disp->digits[HU058_M_ONES] = FONT[m_ones];
}

void hu058_set_temperature(hu058_display_t *disp, int16_t temp,
                           bool fahrenheit) {
  if (temp < -99)
    temp = -99;
  if (temp > 999)
    temp = 999;

  disp->ann[HU058_ANN_DEGREE] = true;
  disp->digits[HU058_M_ONES] = fahrenheit ? SEG_LETTER_F : SEG_LETTER_C;

  bool negative = (temp < 0);
  uint16_t abs_temp = negative ? (uint16_t)(-temp) : (uint16_t)temp;

  uint8_t ones = abs_temp % 10;
  uint8_t tens = (abs_temp / 10) % 10;
  uint8_t hundreds = (abs_temp / 100) % 10;

  disp->digits[HU058_M_TENS] = FONT[ones];

  if (negative) {
    if (tens > 0) {
      disp->digits[HU058_H_ONES] = FONT[tens];
      disp->digits[HU058_H_TENS] = SEG_MINUS;
    } else {
      disp->digits[HU058_H_ONES] = SEG_MINUS;
      disp->digits[HU058_H_TENS] = 0;
    }
  } else {
    disp->digits[HU058_H_ONES] = (hundreds > 0 || tens > 0) ? FONT[tens] : 0;
    disp->digits[HU058_H_TENS] = (hundreds > 0) ? FONT[hundreds] : 0;
  }
}

/*
 * Compile one digit block's contribution to a COM phase.
 *
 * COM-low phases (0, 2): LED3→G, LED4→F, LED5→E
 * COM-high phases (1, 3): LED2→D, LED3→C, LED4→B, LED5→A
 */
static uint16_t compile_digit(uint8_t segs, hu058_color_t color,
                              bool com_high) {
  uint16_t bits = 0;

  if (!com_high) {
    if (segs & HU058_SEG_G)
      bits |= led_bits(3, color);
    if (segs & HU058_SEG_F)
      bits |= led_bits(4, color);
    if (segs & HU058_SEG_E)
      bits |= led_bits(5, color);
  } else {
    if (segs & HU058_SEG_D)
      bits |= led_bits(2, color);
    if (segs & HU058_SEG_C)
      bits |= led_bits(3, color);
    if (segs & HU058_SEG_B)
      bits |= led_bits(4, color);
    if (segs & HU058_SEG_A)
      bits |= led_bits(5, color);
  }

  return bits;
}

void hu058_compile(hu058_display_t *disp) {
  memset(disp->seg_buf, 0, sizeof(disp->seg_buf));

  for (int d = 0; d < HU058_NUM_DIGITS; d++) {
    uint8_t drv = DIGIT_DRIVER[d];
    uint8_t block = DIGIT_BLOCK[d];
    uint8_t segs = disp->digits[d];
    hu058_color_t col = disp->digit_color[d];

    int phase_low = block * 2;
    int phase_high = block * 2 + 1;

    disp->seg_buf[drv][phase_low] |= compile_digit(segs, col, false);
    disp->seg_buf[drv][phase_high] |= compile_digit(segs, col, true);
  }

  for (int a = 0; a < HU058_NUM_ANN; a++) {
    if (!disp->ann[a])
      continue;
    uint8_t drv = ANN_MAP[a].driver;
    uint8_t phase = ANN_MAP[a].phase;
    disp->seg_buf[drv][phase] |= led_bits(1, disp->ann_color[a]);
  }
}

void hu058_scan_step(hu058_display_t *disp) {
  uint8_t phase = disp->scan_phase;

  aip33628_write(disp->drv[0], disp->seg_buf[0][phase], disp->com_buf[phase],
                 disp->current);
  aip33628_write(disp->drv[1], disp->seg_buf[1][phase], disp->com_buf[phase],
                 disp->current);

  disp->scan_phase = (phase + 1) & 3;
}
