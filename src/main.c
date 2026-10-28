#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#include "settings.h"
#include "ds1302.h"
#include "hu058.h"
#include "aip33628.h"
#include "thermistor.h"
#include "lightsensor.h"
#include "speaker.h"
#include "buttons.h"
#if IS_ENABLED(CONFIG_WIFI)
#include "ntp.h"
#include "webconfig.h"
#endif

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* PIO pin assignments for the two AiP33628 LED drivers. */
#define AIP_HOURS_DIN   6
#define AIP_HOURS_CLK   7
#define AIP_MINUTES_DIN 8
#define AIP_MINUTES_CLK 9
#define AIP_PIO_FREQ_HZ 1000000

/* ADC channel specs from devicetree (zephyr,user node). */
static const struct adc_dt_spec adc_therm = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 0);
static const struct adc_dt_spec adc_light = ADC_DT_SPEC_GET_BY_IDX(DT_PATH(zephyr_user), 1);

/* Peripheral instances. */
static ds1302_t rtc = {
	.ce   = GPIO_DT_SPEC_GET(DT_ALIAS(ds1302_ce),   gpios),
	.sclk = GPIO_DT_SPEC_GET(DT_ALIAS(ds1302_sclk), gpios),
	.io   = GPIO_DT_SPEC_GET(DT_ALIAS(ds1302_io),   gpios),
};

static thermistor_config_t therm_cfg = {
	.r_pullup  = 10000.0f,
	.r_nominal = 10000.0f,
	.t_nominal = 25.0f,
	.beta      = 3950.0f,
};

static lightsensor_config_t light_cfg = {
	.r_pullup = 10000.0f,
};

static speaker_t speaker = {
	.gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(buzzer), gpios),
};

static const buttons_config_t btn_cfg = {
	.btn1 = GPIO_DT_SPEC_GET(DT_ALIAS(button1), gpios),
	.btn2 = GPIO_DT_SPEC_GET(DT_ALIAS(button2), gpios),
};

static aip33628_t drv_hours;
static aip33628_t drv_minutes;
static hu058_display_t display;

static settings_t settings;

/* Display scan timer — four phases at ~1.6 kHz = 400 Hz refresh. */
static void scan_timer_handler(struct k_timer *timer);
K_TIMER_DEFINE(scan_timer, scan_timer_handler, NULL);

static void scan_timer_handler(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	hu058_scan_step(&display);
}

static void on_settings_saved(const settings_t *s)
{
	settings_save(s);
	LOG_INF("Settings saved");
}

static void on_button(button_event_t event)
{
	switch (event) {
	case BUTTON_1_PRESS:
		LOG_INF("Button 1 pressed");
		break;
	case BUTTON_2_PRESS:
		LOG_INF("Button 2 pressed");
		break;
	default:
		break;
	}
}

int main(void)
{
	int ret;

	ret = settings_load(&settings);
	if (ret < 0) {
		LOG_WRN("No saved settings, using defaults");
	}

	ret = ds1302_init(&rtc);
	if (ret < 0) {
		LOG_ERR("ds1302_init failed: %d", ret);
	}

	therm_cfg.adc = adc_therm;
	light_cfg.adc = adc_light;
	ret = thermistor_init(&therm_cfg);
	if (ret < 0) {
		LOG_ERR("thermistor_init failed: %d", ret);
	}

	ret = lightsensor_init(&light_cfg);
	if (ret < 0) {
		LOG_ERR("lightsensor_init failed: %d", ret);
	}

	ret = speaker_init(&speaker);
	if (ret < 0) {
		LOG_ERR("speaker_init failed: %d", ret);
	}

	ret = buttons_init(&btn_cfg, on_button);
	if (ret < 0) {
		LOG_ERR("buttons_init failed: %d", ret);
	}

	ret = aip33628_init(&drv_hours, pio1, 0,
			    AIP_HOURS_DIN, AIP_HOURS_CLK, AIP_PIO_FREQ_HZ);
	if (ret < 0) {
		LOG_ERR("aip33628_init hours failed: %d", ret);
	}

	ret = aip33628_init(&drv_minutes, pio1, 1,
			    AIP_MINUTES_DIN, AIP_MINUTES_CLK, AIP_PIO_FREQ_HZ);
	if (ret < 0) {
		LOG_ERR("aip33628_init minutes failed: %d", ret);
	}

	/* Lamp test: bypass hu058 layer, send known-good frames directly.
	 * Red sinks only (SEG3,6,9,12,15) at low current, cycle through COM
	 * pairs. Full white at max current pulls ~1.2 A, far over the board's
	 * 250 mA rating. */
	LOG_INF("=== LAMP TEST START ===");
	for (int rep = 0; rep < 3000; rep++) {
		for (int phase = 0; phase < 4; phase++) {
			static const uint8_t com_test[4] = {0x03, 0x0C, 0x30, 0xC0};
			aip33628_write(&drv_hours, 0x9248, com_test[phase], 5);
			aip33628_write(&drv_minutes, 0x9248, com_test[phase], 5);
			k_usleep(625);
		}
	}
	LOG_INF("=== LAMP TEST END ===");

	hu058_init(&display, &drv_hours, &drv_minutes);
	hu058_set_brightness(&display, settings.brightness);
	hu058_set_time_format(&display,
			      settings.time_24h ? HU058_24H : HU058_12H);

	hu058_color_t color = {
		.r = settings.led_r,
		.g = settings.led_g,
		.b = settings.led_b,
	};
	for (int i = 0; i < HU058_NUM_DIGITS; i++) {
		hu058_set_digit_color(&display, i, color);
	}
	for (int i = 0; i < HU058_NUM_ANN; i++) {
		hu058_set_ann_color(&display, i, color);
	}

	ds1302_time_t now;
	ds1302_get_time(&rtc, &now);
	hu058_set_time(&display, now.hours, now.minutes);
	hu058_set_ann(&display, HU058_ANN_COLON_UPPER, true);
	hu058_set_ann(&display, HU058_ANN_COLON_LOWER, true);
	hu058_compile(&display);

	k_timer_start(&scan_timer, K_USEC(625), K_USEC(625));

#if IS_ENABLED(CONFIG_WIFI)
	ret = webconfig_start(&settings, on_settings_saved);
	if (ret < 0) {
		LOG_ERR("webconfig_start failed: %d", ret);
	}
#endif

	speaker_beep(&speaker, 1000, 100);

	LOG_INF("Entering main loop");

	while (1) {
		ds1302_get_time(&rtc, &now);
		hu058_set_time(&display, now.hours, now.minutes);

		uint8_t brightness = lightsensor_read_brightness(
			&light_cfg, HU058_BRIGHTNESS_1, HU058_BRIGHTNESS_5);
		hu058_set_brightness(&display, brightness);

		hu058_compile(&display);

		k_sleep(K_SECONDS(1));
	}

	return 0;
}
