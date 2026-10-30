# Pico W to HU-058 Wiring

Either a Pico W (RP2040) or Pico 2 W (RP2350A) works. The pinout is
identical; only the MCU differs.

## DIP-16 socket pinout

The Pico W replaces the STC8G1K17-38I in the DIP-16 socket. Pin 1 is
the end marked on the silkscreen.

```text
              ┌───────┐
  CLK_1  1 ───┤ ·     ├─── 16  CLK *
 DATA_1  2 ───┤       ├─── 15  (RXD_2)
    R_T  3 ───┤       ├─── 14  CLK * / DS1302 CE **
    R_P  4 ───┤       ├─── 13  DS1302 IO **
   DATA  5 ───┤       ├─── 12  DS1302 SCLK **
    VCC  6 ───┤       ├─── 11  R_B (buzzer)
     NC  7 ───┤       ├─── 10  S2 (button)
    GND  8 ───┤       ├───  9  S1 (button)
              └───────┘

*  CLK is on pin 16 for HU-058 / HU-058SE,
   or pin 14 for HU-058D.
** DS1302 on pins 12-14 for HU-058 / HU-058SE only
   (absent on HU-058D).
```

## Pico W connections

Seven wires from the Pico W go into the DIP-16 footprint for core
display operation. Additional GPIOs connect to the DS1302 RTC, analog
sensors, and the buzzer.

### Display (AiP33628 drivers via PIO)

| Pico W GPIO | Pico Pin | Socket Pin | Net | Function |
| --- | --- | --- | --- | --- |
| GP6 | 9 | 5 | DATA | AiP33628 driver 1 (hours) data |
| GP7 | 10 | 14 or 16 | CLK | AiP33628 driver 1 (hours) clock |
| GP8 | 11 | 2 | DATA_1 | AiP33628 driver 2 (minutes) data |
| GP9 | 12 | 1 | CLK_1 | AiP33628 driver 2 (minutes) clock |

Driver 1 CLK lands on socket pin 14 (HU-058D) or pin 16 (HU-058 /
HU-058SE). Check which board variant you have.

### Buttons

| Pico W GPIO | Pico Pin | Socket Pin | Net | Function |
| --- | --- | --- | --- | --- |
| GP14 | 19 | 9 | S1 | Button 1 (active low, internal pull-up) |
| GP15 | 20 | 10 | S2 | Button 2 (active low, internal pull-up) |

The buttons switch to ground. The Pico W internal pull-up holds the
lines high when open, replacing the STC8G's quasi-bidirectional port
pull-up.

### Analog sensors

| Pico W GPIO | Pico Pin | Socket Pin | Net | Function |
| --- | --- | --- | --- | --- |
| GP26 (ADC0) | 31 | 3 | R_T | NTC thermistor (10K divider to +5V) |
| GP27 (ADC1) | 32 | 4 | R_P | LDR photoresistor (10K divider to +5V) |

### Buzzer

| Pico W GPIO | Pico Pin | Socket Pin | Net | Function |
| --- | --- | --- | --- | --- |
| GP11 | 15 | 11 | R_B | Buzzer (open-drain, PNP driver Q1 S8550) |

Pin 11 feeds a 10K resistor into the base of a PNP transistor with its
emitter at +5V. Drive open-drain: low turns the buzzer on, high-Z turns
it off.

### DS1302 RTC (via DIP-16 socket)

| Pico W GPIO | Pico Pin | Socket Pin | Signal | Function |
| --- | --- | --- | --- | --- |
| GP3 | 5 | 12 | SCLK | Serial clock |
| GP4 | 6 | 13 | IO | Bidirectional data |
| GP2 | 4 | 14 | CE | Chip enable (active-high RST) |

The DS1302 is present on the HU-058 and HU-058SE boards (with a
32.768 kHz crystal and CR1220 coin cell) and connects through DIP-16
socket pins 12, 13 and 14. It is absent on the HU-058D, where socket
pin 14 is used for the display CLK instead.

### Power and ground

| Pico W | Pico Pin | Socket Pin | Net | Notes |
| --- | --- | --- | --- | --- |
| VSYS | 39 | 6 | VCC (+5V) | Shared 5V rail via header P3 or USB |
| GND | 38 | 8 | GND | Common ground (any GND pin works) |

On the bench, power each board from its own USB and tie grounds through
socket pin 8. In the case, feed the Pico W from header P3 (+5V and
GND).

## Wiring diagram

```text
  Pico W / Pico 2 W                 HU-058 DIP-16 Socket
  ┌─────────────────┐              ┌───────────────────┐
  │  (pin 9)   GP6  ├─────────────►│ Pin 5   DATA      │
  │  (pin 10)  GP7  ├─────────────►│ Pin 16  CLK       │  AiP33628
  │  (pin 11)  GP8  ├─────────────►│ Pin 2   DATA_1    │  driver 1 (hours)
  │  (pin 12)  GP9  ├─────────────►│ Pin 1   CLK_1     │  driver 2 (minutes)
  │                 │              │                   │
  │  (pin 19)  GP14 ├─────────────►│ Pin 9   S1        │  buttons
  │  (pin 20)  GP15 ├─────────────►│ Pin 10  S2        │
  │                 │              │                   │
  │  (pin 31)  GP26 ├─────────────►│ Pin 3   R_T       │  NTC thermistor
  │  (pin 32)  GP27 ├─────────────►│ Pin 4   R_P       │  LDR light sensor
  │  (pin 15)  GP11 ├─────────────►│ Pin 11  R_B       │  buzzer
  │                 │              │                   │
  │  (pin 39)  VSYS ├─────────────►│ Pin 6   VCC (+5V) │
  │  (pin 38)  GND  ├─────────────►│ Pin 8   GND       │
  │                 │              │                   │
  │  (pin 4)   GP2  ├─────────────►│ Pin 14  CE        │  DS1302 RTC
  │  (pin 5)   GP3  ├─────────────►│ Pin 12  SCLK      │  (via socket)
  │  (pin 6)   GP4  ├─────────────►│ Pin 13  IO        │
  │                 │              │                   │
  └─────────────────┘              └───────────────────┘
```

## Logic levels

The 3.3V outputs of the Pico W drive the AiP33628 at 5V without a
level shifter. The datasheet requires 0.7 x VDD (3.5V) for a logic
high, but the parts switch near half rail in practice. If you see
intermittent flicker or wrong digits, a 74AHCT125 on the four display
lines is the fix.

## Socket pins not used

| Socket Pin | STC Port | Net | Notes |
| --- | --- | --- | --- |
| 7 | P5.5 | NC | Not connected on PCB |
| 15 | P3.6 | RXD_2 | ESP-01S serial (not used with Pico W) |
