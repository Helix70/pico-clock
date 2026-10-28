# HU-058 Clock Project — Session Summary

## Project overview

A clock built on the **Raspberry Pi Pico 2 W** (RP2350A, Cortex-M33, 4 MB
flash) running **Zephyr RTOS v4.5.0-rc3**, targeting the **HU-058 clock PCB**
with the original microcontroller removed. The PCB has two AiP33628 LED drivers
controlling four seven-segment RGB digits plus annunciators (AM, colon, dash,
degree), a DS1302 battery-backed RTC, an NTC thermistor, an LDR light sensor,
a piezo speaker, and two push buttons.

All source files live in `src/` as standalone `.h`/`.c` pairs. The board overlay
is `boards/rpi_pico2_rp2350a_m33_0_w.overlay` and Kconfig is `prj.conf`.

Board target: `rpi_pico2/rp2350a/m33_0/w` (the older `m33` form without `_0`
still works but triggers a deprecation warning).

## Build status

**Build succeeds.** The project compiles cleanly with:

```text
west build -p always -b rpi_pico2/rp2350a/m33/w
```

Output: `build/zephyr/zephyr.uf2` (~1 MB).

**Do not** use `-S cdc-acm-console` — it was previously included but causes
USB CDC to capture the console, dropping characters. All logging now goes to
UART0 (physical serial) via `CONFIG_LOG_BACKEND_UART`.

West executable location:
`C:/Users/z0035sjy/.pico-sdk/zephyr_workspace/venv/Scripts/west.exe`

ZEPHYR_BASE:
`C:/Users/z0035sjy/.pico-sdk/zephyr_workspace/zephyr-main`

## Drivers completed

- **AiP33628 LED** (`aip33628.h/c`, `.pio`, `.pio.h`) —
  PIO-driven LED matrix driver
- **HU-058 display** (`hu058.h/c`) —
  4-digit 7-seg + annunciators, scan-multiplexed
- **DS1302 RTC** (`ds1302.h/c`, `gpio_dt_spec`) —
  Battery-backed clock, 3-wire bit-bang
- **Thermistor** (`thermistor.h/c`, `adc_dt_spec`) —
  NTC via voltage divider + beta equation
- **Light sensor** (`lightsensor.h/c`, `adc_dt_spec`) —
  LDR reading, auto-brightness mapping
- **Speaker** (`speaker.h/c`, `pwm_dt_spec`) —
  Tone generation via PWM, blocking beep
- **NTP** (`ntp.h/c`, `sntp_simple()`) —
  Time sync, returns `ds1302_time_t`
- **WiFi AP** (`wifiap.h/c`, `net_mgmt`) —
  Open AP mode for configuration portal
- **HTTP server** (`httpserver.h/c`, `zsock_*`) —
  Single-client BSD socket listener in thread
- **Web config** (`webconfig.h/c`) —
  HTML form portal (orchestrates wifiap + httpserver)
- **Settings** (`settings.h/c`, `nvs_*`) —
  NVS flash persistence for all user settings
- **Buttons** (`buttons.h/c`, `gpio_dt_spec`/`k_timer`) —
  2-button poll driver with debounce + hold

## Driver patterns

All drivers follow a consistent pattern:

- Config struct with Zephyr `_dt_spec` members (gpio, adc, pwm)
- `<module>_init()` returns `int` (0 success, negative errno)
- Const config pointers where the driver doesn't modify config
- Synchronous/polling (except buttons which use `k_timer` callback)

## Devicetree overlay (`boards/rpi_pico2_rp2350a_m33_0_w.overlay`)

```text
Root nodes:
  ds1302       — gpio-keys for CE (GP2), SCLK (GP3), IO (GP4)
  buttons      — gpio-keys for btn1 (GP14), btn2 (GP15) [PLACEHOLDER PINS]
  zephyr,user  — io-channels for ADC ch0 (thermistor) and ch1 (light sensor)
  speaker-pwm  — pwm-leds consumer node for speaker on PWM ch5b (GP11)

Aliases:
  ds1302-ce, ds1302-sclk, ds1302-io
  button1, button2

&pinctrl:
  pwm_ch5b_default — GP11 mapped to PWM_5B_P11

&adc:
  channel@0    — thermistor (GP26)
  channel@1    — light sensor (GP27)

&pwm:
  pinctrl-0 includes pwm_ch4b_default (onboard LED) + pwm_ch5b_default (speaker)

&flash0:
  storage_partition@3fa000 — 24 KB NVS at end of 4 MB flash
```

## Kconfig (`prj.conf`)

Enabled: GPIO, ADC, PWM, newlib, networking (sockets, SNTP, DNS, WiFi,
IPv4, DHCP client/server, TCP, UDP), NVS (flash, flash_map,
flash_page_layout), thread custom data, static IP 192.168.4.1 for AP mode.

Logging: `CONFIG_LOG_MODE_IMMEDIATE` over `CONFIG_LOG_BACKEND_UART` on UART0.
Shell is commented out until WiFi debugging is complete.

## Settings system

`settings_t` is the single unified struct used by both NVS persistence and the
web config portal. Fields:

- `wifi_ssid` / `wifi_pass` — station credentials
- `fahrenheit`, `time_24h`, `hourly_beep` — boolean preferences (packed into
  one NVS byte)
- `alarms[4]` — hour/minute/enabled per alarm
- `ntp_server` — 65-byte buffer (default `pool.ntp.org`)
- `utc_offset_sec` — signed seconds, web form presents
  whole hours (-12 to +14)
- `brightness` — raw 0-15 value; web form shows perceptual levels 1-5 via
  lookup table `{0, 1, 3, 8, 15}`
- `led_r`, `led_g`, `led_b` — color booleans
  (web form: radio white/red/green/blue)

NVS ID scheme: magic (0x00), SSID (0x01), pass (0x02), prefs byte (0x03),
alarms (0x04), NTP server (0x05), UTC offset (0x06), display HW byte (0x07).

First-boot detected by magic byte; `set_defaults()` fills sensible values.

NVS API uses `<zephyr/kvss/nvs.h>` with `PARTITION_DEVICE()`,
`PARTITION_OFFSET()`, `PARTITION_SIZE()` (the non-deprecated API).

## Web config portal

`webconfig_start()` enables AP mode (`HU058-Clock` SSID, channel 6, open) and
starts an HTTP server on port 80. The HTML form has fieldsets for:

- WiFi (SSID + password)
- Display (fahrenheit checkbox, 24h checkbox)
- Hourly Beep (checkbox)
- NTP (server text + UTC offset number)
- LED Display (brightness dropdown 1-5 + color radio white/red/green/blue)
- Alarms (4 rows, hour/minute/enabled)

POST handler parses URL-encoded form body, updates `settings_t`, calls
`on_save` callback. HTML is generated in chunks via `snprintf` into a 512-byte
stack buffer to stay within embedded constraints.

## Button driver

Poll-based using `k_timer` at 20 ms intervals:

- 3-sample debounce (60 ms settling) per button
- Press/release callbacks on state transitions
- `BUTTON_BOTH_HELD` fires once when both buttons held for 2 seconds
- Intended use: holding both buttons triggers AP mode for reconfiguration
- **GPIO 14/15 are placeholders** — replace with actual pin assignments
- Callbacks fire from timer context; callers should defer heavy work

## main.c — application wiring

`main.c` ties all drivers together:

- Loads settings from NVS
- Initialises DS1302 RTC, thermistor, light sensor, speaker, buttons
- Initialises two AiP33628 PIO drivers (hours on GP6/7, minutes on GP8/9)
- Sets up HU-058 display with saved colour/brightness/time format
- Starts 625 us scan timer (~1.6 kHz multiplex refresh)
- Starts WiFi AP + web config portal
- Plays startup beep
- Main loop: reads RTC + light sensor every second, updates display

ADC specs use `DT_PATH(zephyr_user)` with `ADC_DT_SPEC_GET_BY_IDX`.
PWM speaker uses `PWM_DT_SPEC_GET(DT_NODELABEL(speaker))`.

## PIO header (`src/aip33628.pio.h`)

Hand-assembled from `aip33628.pio` because pioasm is not available in the
Zephyr build environment. Contains 13 instructions with side-set=1,
wrap_target=0, wrap=12. Includes the PIO program struct, default config
helper, and init function from the `.pio` c-sdk block.

## Build fixes applied (RP2040 to RP2350 migration)

1. **Overlay filename** — renamed from `rpi_pico_w.overlay` to
   `rpi_pico2_rp2350a_m33_0_w.overlay` to match the board qualifier
2. **ADC address** — changed from `DT_PATH(soc, adc_4004c000)` (RP2040) to
   `DT_PATH(zephyr_user)` with `io-channels` property
3. **PWM reference** — changed from `PWM_DT_SPEC_GET(DT_NODELABEL(pwm))`
   (controller) to `PWM_DT_SPEC_GET(DT_NODELABEL(speaker))` (consumer node
   under `pwm-leds`)
4. **PIO header** — created `src/aip33628.pio.h` with hand-assembled
   instructions
5. **NVS API** — migrated from deprecated `<zephyr/fs/nvs.h>` +
   `FIXED_PARTITION_ID` to `<zephyr/kvss/nvs.h>` + `PARTITION_DEVICE` /
   `PARTITION_OFFSET` / `PARTITION_SIZE`
6. **Flash partition offset** — changed from `0x1fa000` (end of 2 MB) to
   `0x3fa000` (end of 4 MB for Pico 2 W)

## Runtime fixes applied

1. **Firmware crash from blocking PIO write in ISR** — `aip33628_write_raw()`
   called `pio_sm_put()` from the scan timer ISR, which blocked when the FIFO
   was full and caused a hard fault. Fixed by guarding with
   `pio_sm_is_tx_fifo_full()` and silently dropping the frame if full.
2. **USB CDC dropping characters** — logging routed to USB CDC would lose
   output. Switched all logging to UART0 (physical serial) via
   `CONFIG_LOG_BACKEND_UART` / `CONFIG_UART_CONSOLE` in `prj.conf`.
3. **Console still on CDC after overlay** — removed `-S cdc-acm-console` from
   build tasks in `.vscode/tasks.json`.
4. **Wrong webconfig_start signature** — updated call to
   `webconfig_start(&settings, on_settings_saved)`.
5. **30-second boot delay** — was actually the WiFi SPI FEEDBEAD timeout, not
   a boot delay issue. See "WiFi FEEDBEAD SPI failure" below.

## WiFi FEEDBEAD SPI failure (OPEN — primary blocker)

The CYW43439 WiFi chip reads `0x00000000` instead of `0xFEEDBEAD` from
`SPI_READ_TEST_REGISTER` during initialization. This prevents WiFi AP mode
from starting, which blocks the web configuration portal.

### What has been ruled out

- **Board target qualifier** — using the correct `/w` variant
  (`rpi_pico2/rp2350a/m33_0/w`)
- **Missing firmware blobs** — both `43439A0.bin` and the Murata-1YN
  `43439A0.clm_blob` are present on disk under
  `modules/hal/infineon/zephyr/blobs/`
- **PIO0 conflict** — AiP33628 LED drivers use PIO1; WiFi uses PIO0; no
  conflict
- **`input_sync_bypass` register** — confirmed to exist on RP2350 PIO struct
- **GPIO function select for PIO0** — `pio_gpio_init()` uses
  `PIO_FUNCSEL_NUM` which resolves to `GPIO_FUNC_PIO0 = 6` on both RP2040
  and RP2350; no mismatch
- **PIO clock divider at 150 MHz** — divider calculates to 7.5 for 10 MHz
  target (150 MHz / (2 cycles × 10 MHz)); valid fractional divider, well
  within CYW43439's 50 MHz max

### Init sequence traced

1. GPIO24 set to GPIO output LOW (bus-select, tells CYW43439 to use SPI)
2. GPIO23 LOW for 10 ms (CBUCK discharge), then HIGH with 250 ms power-up
   delay
3. `whd_init` → `whd_bus_spi_attach` → `whd_wifi_on` → FEEDBEAD read
4. Each SPI transfer toggles GPIO24 between PIO0 mode (for SPI data via
   `airoc_wifi_default` pinctrl) and GPIO mode (for host-wake IRQ via
   `airoc_wifi_host_wake` pinctrl)
5. PIO SPI uses half-duplex SIO mode 0,0: separate TX (3 instr, 2
   cycles/bit) and RX (4 instr, 2 cycles/bit) PIO programs, switching
   GPIO24 direction between phases

### Remaining suspects

- **Pinctrl state application ordering** — the AIROC driver and PIO SPI
  driver both touch GPIO24 configuration; potential race or conflict on the
  very first SPI transaction
- **RP2350-specific PIO SPI driver gaps** — minimal RP2350-specific code in
  the driver; possible missing adaptations
- **Known Zephyr issues** — found relevant upstream issues:
  - [#91226](https://github.com/zephyrproject-rtos/zephyr/issues/91226) —
    "Pico 2W cannot use wifi due to hardcoded setting in AIROC driver"
    (fixed in PR #91227, may not be in v4.5.0-rc3)
  - [#117115](https://github.com/zephyrproject-rtos/zephyr/issues/117115) —
    "Pico-W and Pico2-W GPIO Driver not reliably working due to missing
    second parameter" (closed)
  - [#104719](https://github.com/zephyrproject-rtos/zephyr/issues/104719) —
    "Pico 2W: wifi ap enable fails" (closed as stale, unofficial workaround
    only)

### Next steps

1. Read the fix from PR #91227 and check if it is included in the local
   Zephyr v4.5.0-rc3 tree
2. Read issue #117115 for the GPIO driver fix — may directly affect GPIO24
   reliability
3. Add debug logging to the SPI transfer path to confirm GPIO24 and GPIO29
   are properly configured when the FEEDBEAD read occurs
4. Consider updating Zephyr to a newer release if fixes are missing

## What's not built yet

- Actual AP-mode revert logic wired to `BUTTON_BOTH_HELD`
- Station WiFi connection + NTP sync flow
- Alarm trigger logic
- Hourly beep logic

## Key design decisions

- **NVS over Zephyr Settings** — simpler for a small fixed field set
- **Unified settings_t** — shared between NVS and webconfig, no mapping code
- **LED color as three bools** — keeps `settings.h` free
  from hardware includes
- **NTP server as inline buffer** — 65 bytes in struct for NVS persistence
  (ntp.h uses `const char *` pointer, populated from settings buffer)
- **Brightness raw value in NVS** — web form does perceptual-to-raw mapping
- **Poll-based buttons** — consistent with project's synchronous style, first
  driver to use callbacks (via `k_timer`)
- **Split HTML generation** — large fieldsets sent across multiple
  `snprintf`/`httpserver_send` calls to fit in 512-byte stack buffer
- **zephyr,user node for ADC** — idiomatic Zephyr pattern, avoids
  hard-coding SoC-specific ADC addresses
- **pwm-leds consumer node** — `PWM_DT_SPEC_GET` requires a consumer, not
  the PWM controller

## Pin assignments (current placeholders)

| Function | GPIO | Notes |
| --- | --- | --- |
| DS1302 CE | GP2 | Confirmed |
| DS1302 SCLK | GP3 | Confirmed |
| DS1302 IO | GP4 | Confirmed |
| AiP33628 Hours DIN | GP6 | Runtime PIO config |
| AiP33628 Hours CLK | GP7 | Runtime PIO config |
| AiP33628 Minutes DIN | GP8 | Runtime PIO config |
| AiP33628 Minutes CLK | GP9 | Runtime PIO config |
| Speaker | GP11 (PWM 5B) | Via pinctrl overlay |
| Button 1 | GP14 | **Placeholder** |
| Button 2 | GP15 | **Placeholder** |
| Thermistor | GP26 (ADC0) | Confirmed |
| Light sensor | GP27 (ADC1) | Confirmed |
