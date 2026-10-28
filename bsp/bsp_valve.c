/*
 * bsp_valve.c — 阀门/继电器控制实现
 *
 * 硬件: 19 路继电器阀门, active-low 驱动
 *   - GPIO 输出 LOW → 继电器吸合 → 阀门打开
 *   - GPIO 输出 HIGH → 继电器释放 → 阀门关闭
 *
 * 索引约定: valve_specs[0] 保留, 阀门从 [1] 开始 (1-indexed)
 */

#include "bsp_valve.h"
#include "bsp_gpio_map.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/* 阀门 GPIO 设备树规格表 (索引 = 阀门编号, [0] 保留) */
static const struct gpio_dt_spec valve_specs[] = {
	[0]  = {0}, /* unused, valves are 1-indexed */
	[1]  = VALVE_Y1,  [2]  = VALVE_Y2,  [3]  = VALVE_Y3,
	[4]  = VALVE_Y4,  [5]  = VALVE_Y5,  [6]  = VALVE_Y6,
	[7]  = VALVE_Y7,  [8]  = VALVE_Y8,  [9]  = VALVE_Y9,
	[10] = VALVE_Y10, [11] = VALVE_Y11, [12] = VALVE_Y12,
	[13] = VALVE_Y13, [14] = VALVE_Y14, [15] = VALVE_Y15,
	[16] = VALVE_Y16, [17] = VALVE_Y17, [18] = VALVE_Y18,
	[19] = VALVE_Y19,
};

int bsp_valve_init(void)
{
	for (int i = 1; i < VALVE_COUNT; i++) {
		if (!gpio_is_ready_dt(&valve_specs[i])) {
			printk("Valve Y%d GPIO not ready\n", i);
			return -1;
		}
		gpio_pin_configure_dt(&valve_specs[i], GPIO_OUTPUT_INACTIVE);
	}
	printk("Valves initialized: Y1-Y19 (all OFF)\n");
	return 0;
}

void bsp_valve_open(valve_name_t valve)
{
	if (valve > 0 && valve < VALVE_COUNT) {
		gpio_pin_set_dt(&valve_specs[valve], 1);
	}
}

void bsp_valve_close(valve_name_t valve)
{
	if (valve > 0 && valve < VALVE_COUNT) {
		gpio_pin_set_dt(&valve_specs[valve], 0);
	}
}

void bsp_valve_open_all(void)
{
	for (int i = 1; i < VALVE_COUNT; i++) {
		gpio_pin_set_dt(&valve_specs[i], 1);
	}
}

void bsp_valve_close_all(void)
{
	for (int i = 1; i < VALVE_COUNT; i++) {
		gpio_pin_set_dt(&valve_specs[i], 0);
	}
}

void bsp_valve_open_mask(uint32_t mask)
{
	for (int i = 1; i < VALVE_COUNT; i++) {
		if (mask & (1u << (i - 1))) {
			gpio_pin_set_dt(&valve_specs[i], 1);
		}
	}
}

void bsp_valve_close_mask(uint32_t mask)
{
	for (int i = 1; i < VALVE_COUNT; i++) {
		if (mask & (1u << (i - 1))) {
			gpio_pin_set_dt(&valve_specs[i], 0);
		}
	}
}
