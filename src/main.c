#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#include "settings.h"
#include "ds1302.h"
#include "hu058.h"
#include "aip33628.h"
#include "thermistor.h"
#include "lightsensor.h"
#include "speaker.h"
#include "buttons.h"
#include <pico/bootrom.h>
#if IS_ENABLED(CONFIG_WIFI)
#include "ntp.h"
#include "wifista.h"
#include "wifisetup.h"
#include "webconfig.h"
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>
#endif

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* PIO pin assignments for the two AiP33628 LED drivers. */
#define AIP_HOURS_DIN   6
#define AIP_HOURS_CLK   7
#define AIP_MINUTES_DIN 8
#define AIP_MINUTES_CLK 9
#define AIP_PIO_FREQ_HZ 1000000

/* How long to wait at boot for a host to open the USB CDC console. */
#define CONSOLE_WAIT_MS 3000

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

#if IS_ENABLED(CONFIG_WIFI)
static volatile bool wifi_reconfig_requested;
#endif
static volatile bool show_temp;

static void on_button(button_event_t event)
{
	switch (event) {
	case BUTTON_1_PRESS:
		LOG_INF("Button 1 pressed");
		break;
	case BUTTON_1_HELD:
		LOG_INF("Button 1 held");
#if IS_ENABLED(CONFIG_WIFI)
		wifi_reconfig_requested = true;
#endif
		break;
	case BUTTON_2_PRESS:
		LOG_INF("Button 2 pressed");
		show_temp = !show_temp;
		break;
	case BUTTON_BOTH_HELD:
		k_timer_stop(&scan_timer);
		for (int i = 0; i < HU058_NUM_DIGITS; i++) {
			hu058_set_digit_color(&display, i, HU058_COLOR_RED);
		}
		for (int i = 0; i < HU058_NUM_ANN; i++) {
			hu058_set_ann_color(&display, i, HU058_COLOR_RED);
		}
		hu058_set_segments(&display, HU058_H_TENS,
				   HU058_SEG_C | HU058_SEG_D | HU058_SEG_E |
				   HU058_SEG_F | HU058_SEG_G);
		hu058_set_segments(&display, HU058_H_ONES,
				   HU058_SEG_C | HU058_SEG_D | HU058_SEG_E |
				   HU058_SEG_G);
		hu058_set_segments(&display, HU058_M_TENS,
				   HU058_SEG_C | HU058_SEG_D | HU058_SEG_E |
				   HU058_SEG_G);
		hu058_set_segments(&display, HU058_M_ONES,
				   HU058_SEG_D | HU058_SEG_E | HU058_SEG_F |
				   HU058_SEG_G);
		hu058_set_ann(&display, HU058_ANN_COLON_UPPER, false);
		hu058_set_ann(&display, HU058_ANN_COLON_LOWER, false);
		hu058_compile(&display);
		/* Manually drive the display — scan timer ISR can't fire
		   while we're already in ISR context. */
		for (int rep = 0; rep < 320; rep++) {
			hu058_scan_step(&display);
			k_busy_wait(625);
		}
		reset_usb_boot(0, 0);
		break;
	default:
		break;
	}
}

#if IS_ENABLED(CONFIG_WIFI)
static bool wait_for_dhcp(int timeout_sec)
{
	struct net_if *iface = net_if_get_default();

	for (int i = 0; i < timeout_sec * 10; i++) {
		struct net_in_addr *addr =
			net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
		if (addr) {
			printk("IP: %u.%u.%u.%u\n",
			       addr->s4_addr[0], addr->s4_addr[1],
			       addr->s4_addr[2], addr->s4_addr[3]);
			return true;
		}
		k_msleep(100);
	}
	return false;
}

static void apply_display_settings(hu058_display_t *disp, const settings_t *s)
{
	hu058_set_brightness(disp, s->brightness);
	hu058_set_time_format(disp, s->time_24h ? HU058_24H : HU058_12H);

	hu058_color_t color = {
		.r = s->led_r,
		.g = s->led_g,
		.b = s->led_b,
	};
	for (int i = 0; i < HU058_NUM_DIGITS; i++) {
		hu058_set_digit_color(disp, i, color);
	}
	for (int i = 0; i < HU058_NUM_ANN; i++) {
		hu058_set_ann_color(disp, i, color);
	}
}

static void on_settings_saved(const settings_t *s)
{
	settings_save(s);
	apply_display_settings(&display, s);
	hu058_compile(&display);
	printk("Settings saved via web UI\n");
}

static void wifi_connect_and_sync(settings_t *s, ds1302_t *rtc_dev)
{
	int ret;

	if (s->wifi_ssid[0] != '\0') {
		printk("Connecting to %s...\n", s->wifi_ssid);
		ret = wifista_connect(s->wifi_ssid, s->wifi_pass);
		if (ret == 0) {
			printk("Connected!\n");
		} else {
			printk("Failed (%d). Starting WiFi setup.\n", ret);
			wifisetup_run(s);
		}
	} else {
		wifisetup_run(s);
	}

	printk("Waiting for DHCP...\n");
	if (!wait_for_dhcp(15)) {
		printk("DHCP timed out.\n");
		return;
	}

	const char *ntp_srv = s->ntp_server[0] ? s->ntp_server : "pool.ntp.org";
	ntp_config_t ntp_cfg = {
		.server = ntp_srv,
		.utc_offset_sec = s->utc_offset_sec,
		.timeout_ms = 5000,
	};
	ds1302_time_t ntp_time;

	printk("Syncing time from %s...\n", ntp_srv);
	ret = ntp_sync(&ntp_cfg, &ntp_time);
	if (ret == 0) {
		ds1302_set_time(rtc_dev, &ntp_time);
		printk("Time set: %02u:%02u:%02u\n",
		       ntp_time.hours, ntp_time.minutes, ntp_time.seconds);
	} else {
		printk("NTP sync failed (%d), using RTC time.\n", ret);
	}

	ret = webconfig_start(s, on_settings_saved);
	if (ret == 0) {
		struct net_in_addr *addr =
			net_if_ipv4_get_global_addr(net_if_get_default(),
						    NET_ADDR_PREFERRED);
		if (addr) {
			printk("Settings: http://%u.%u.%u.%u/\n",
			       addr->s4_addr[0], addr->s4_addr[1],
			       addr->s4_addr[2], addr->s4_addr[3]);
		}
	} else {
		printk("Web config failed (%d)\n", ret);
	}
}
#endif

/*
 * Wait for a host to open the USB CDC console (DTR asserted) so boot logs
 * aren't lost. Gives up after CONSOLE_WAIT_MS so the clock still runs
 * standalone.
 */
static void wait_for_console(void)
{
	const struct device *console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;

	if (!device_is_ready(console)) {
		return;
	}

	for (int ms = 0; ms < CONSOLE_WAIT_MS; ms += 100) {
		if (uart_line_ctrl_get(console, UART_LINE_CTRL_DTR, &dtr) == 0 && dtr) {
			k_msleep(200);
			break;
		}
		k_msleep(100);
	}
}

int main(void)
{
	int ret;

	wait_for_console();

	printk("[main] settings_load\n");
	ret = settings_load(&settings);
	if (ret < 0) {
		printk("[main] No saved settings, using defaults\n");
	}

	printk("[main] ds1302_init\n");
	ret = ds1302_init(&rtc);
	if (ret < 0) {
		printk("[main] ds1302_init failed: %d\n", ret);
	}

	therm_cfg.adc = adc_therm;
	light_cfg.adc = adc_light;
	printk("[main] thermistor_init\n");
	ret = thermistor_init(&therm_cfg);
	if (ret < 0) {
		printk("[main] thermistor_init failed: %d\n", ret);
	}

	printk("[main] lightsensor_init\n");
	ret = lightsensor_init(&light_cfg);
	if (ret < 0) {
		printk("[main] lightsensor_init failed: %d\n", ret);
	}

	printk("[main] speaker_init\n");
	ret = speaker_init(&speaker);
	if (ret < 0) {
		printk("[main] speaker_init failed: %d\n", ret);
	}

	printk("[main] buttons_init\n");
	ret = buttons_init(&btn_cfg, on_button);
	if (ret < 0) {
		printk("[main] buttons_init failed: %d\n", ret);
	}

	printk("[main] aip33628_init hours\n");
	ret = aip33628_init(&drv_hours, pio1, 0,
			    AIP_HOURS_DIN, AIP_HOURS_CLK, AIP_PIO_FREQ_HZ);
	if (ret < 0) {
		LOG_ERR("aip33628_init hours failed: %d", ret);
	}

	printk("[main] aip33628_init minutes\n");
	ret = aip33628_init(&drv_minutes, pio1, 1,
			    AIP_MINUTES_DIN, AIP_MINUTES_CLK, AIP_PIO_FREQ_HZ);
	if (ret < 0) {
		printk("[main] aip33628_init minutes failed: %d\n", ret);
	}

	printk("[main] lamp test start\n");
	for (int rep = 0; rep < 3000; rep++) {
		for (int phase = 0; phase < 4; phase++) {
			static const uint8_t com_test[4] = {0x03, 0x0C, 0x30, 0xC0};
			aip33628_write(&drv_hours, 0x9248, com_test[phase], 5);
			aip33628_write(&drv_minutes, 0x9248, com_test[phase], 5);
			k_usleep(625);
		}
	}
	printk("[main] lamp test end\n");

	hu058_init(&display, &drv_hours, &drv_minutes);
	apply_display_settings(&display, &settings);

	ds1302_time_t now;
	ds1302_get_time(&rtc, &now);
	hu058_set_time(&display, now.hours, now.minutes);
	hu058_set_ann(&display, HU058_ANN_COLON_UPPER, true);
	hu058_set_ann(&display, HU058_ANN_COLON_LOWER, true);
	hu058_compile(&display);

	k_timer_start(&scan_timer, K_USEC(625), K_USEC(625));

	printk("[main] display ready\n");

#if IS_ENABLED(CONFIG_WIFI)
	printk("[main] wifi_connect_and_sync\n");
	wifi_connect_and_sync(&settings, &rtc);
	printk("[main] wifi done\n");
#endif

	speaker_beep(&speaker, 1000, 100);

	printk("[main] entering main loop\n");

	while (1) {
#if IS_ENABLED(CONFIG_WIFI)
		if (wifi_reconfig_requested) {
			wifi_reconfig_requested = false;
			webconfig_stop();
			wifista_disconnect();
			settings.wifi_ssid[0] = '\0';
			settings.wifi_pass[0] = '\0';
			wifi_connect_and_sync(&settings, &rtc);
		}
#endif
		if (show_temp) {
			float temp_c = thermistor_read_c(&therm_cfg);
			temp_c += (float)settings.temp_offset_tenths / 10.0f;
			int16_t temp_val;
			if (settings.fahrenheit) {
				temp_val = (int16_t)(temp_c * 9.0f / 5.0f + 32.0f + 0.5f);
			} else {
				temp_val = (int16_t)(temp_c + (temp_c >= 0 ? 0.5f : -0.5f));
			}
			hu058_set_temperature(&display, temp_val, settings.fahrenheit);
		} else {
			ds1302_get_time(&rtc, &now);
			hu058_set_time(&display, now.hours, now.minutes);
		}

		hu058_compile(&display);

		k_sleep(K_SECONDS(1));
	}

	return 0;
}
