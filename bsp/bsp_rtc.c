/*
 * bsp_rtc.c — RTC 时间功能
 *
 * Zephyr 已将 STM32 RTC 硬件用作系统 idle timer (tickless idle)。
 * RTC 硬件持续运行, 通过 k_uptime 可获取精确的系统运行时间。
 */

#include "bsp/bsp_rtc.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int bsp_rtc_init(void)
{
	printk("RTC initialized (running as Zephyr idle timer)\n");
	return 0;
}

uint32_t bsp_rtc_get_ticks(void)
{
	return k_uptime_get_32();
}
