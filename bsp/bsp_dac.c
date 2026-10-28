/*
 * bsp_dac.c — DAC 输出控制
 */

#include "bsp/bsp_dac.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/dac.h>
#include <zephyr/sys/printk.h>

static const struct device *dac_dev;

int bsp_dac_init(void)
{
	dac_dev = DEVICE_DT_GET(DT_NODELABEL(dac1));
	if (!device_is_ready(dac_dev)) {
		printk("DAC1 not ready\n");
		return -1;
	}
	printk("DAC initialized: PA4(current) PA5(LED)\n");
	return 0;
}

void bsp_dac_set_current(uint16_t value)
{
	if (value > DAC_MAX_VALUE) value = DAC_MAX_VALUE;
	dac_write_value(dac_dev, DAC_CH_CURRENT, value);
}

void bsp_dac_set_led(uint16_t value)
{
	if (value > DAC_MAX_VALUE) value = DAC_MAX_VALUE;
	dac_write_value(dac_dev, DAC_CH_LED, value);
}
