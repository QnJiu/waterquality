/*
 * bsp_pump.c — 步进电机 / 蠕动泵控制实现
 *
 * 控制链路:
 *   bsp_pump_set_direction() → DIR 引脚 (PD14)
 *   bsp_pump_enable()        → EN 引脚 (PD15, active-low)
 *   bsp_pwm_start_motor()    → PWM 脉冲 (PD13, TIM4 CH2)
 *
 * 使用顺序: set_direction → pwm_start_motor → enable
 * 停止顺序: disable → pwm_stop_motor
 */

#include "bsp_pump.h"
#include "bsp_gpio_map.h"
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/* 电机方向和使能引脚 (设备树定义) */
static const struct gpio_dt_spec motor_dir = MOTOR_DIR_GPIO;
static const struct gpio_dt_spec motor_en  = MOTOR_EN_GPIO;

int bsp_pump_init(void)
{
	if (!gpio_is_ready_dt(&motor_dir)) {
		printk("Motor DIR GPIO not ready\n");
		return -1;
	}
	if (!gpio_is_ready_dt(&motor_en)) {
		printk("Motor EN GPIO not ready\n");
		return -1;
	}

	/* DIR 初始为低 (CCW), EN 初始高 (禁用) */
	gpio_pin_configure_dt(&motor_dir, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&motor_en,  GPIO_OUTPUT_INACTIVE);

	printk("Pump motor initialized: DIR=CCW, EN=DISABLED\n");
	return 0;
}

void bsp_pump_set_direction(bool cw)
{
	gpio_pin_set_dt(&motor_dir, cw ? 1 : 0);
}

void bsp_pump_enable(void)
{
	gpio_pin_set_dt(&motor_en, 1);  /* active-low: 1 = enabled */
}

void bsp_pump_disable(void)
{
	gpio_pin_set_dt(&motor_en, 0);  /* active-low: 0 = disabled */
}
