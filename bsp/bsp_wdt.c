/*
 * bsp_wdt.c — IWDG 看门狗
 */

#include "bsp/bsp_wdt.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/printk.h>

static const struct device *wdt_dev;
static int wdt_channel;

int bsp_wdt_init(void)
{
	wdt_dev = DEVICE_DT_GET(DT_NODELABEL(iwdg));
	if (!device_is_ready(wdt_dev)) {
		printk("IWDG not ready\n");
		return -1;
	}

	struct wdt_timeout_cfg wdt_cfg = {
		.window.min = 0,
		.window.max = 10000,  /* 10 秒超时 */
		.callback = NULL,
	};

	wdt_channel = wdt_install_timeout(wdt_dev, &wdt_cfg);
	if (wdt_channel < 0) {
		printk("IWDG install timeout failed: %d\n", wdt_channel);
		return -1;
	}

	int ret = wdt_setup(wdt_dev, WDT_FLAG_RESET_SOC);
	if (ret < 0) {
		printk("IWDG setup failed: %d\n", ret);
		return -1;
	}

	printk("IWDG initialized: 4s timeout\n");
	return 0;
}

int bsp_wdt_feed(void)
{
	return wdt_feed(wdt_dev, wdt_channel);
}
