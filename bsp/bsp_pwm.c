/*
 * bsp_pwm.c — PWM 输出控制
 */

#include "bsp/bsp_pwm.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

/* PWM 设备指针 */
static const struct device *pwm_devs[PWM_COUNT];
/* 每个通道的 channel 号 */
static const int pwm_channels[PWM_COUNT] = { 1, 1, 3, 2 };

/* 加热 PWM 极性: active-low (PWM_POLARITY_INVERTED 表示低电平为激活) */
#define HEAT_FLAGS  PWM_POLARITY_INVERTED
#define LED_FLAGS   0
#define MOTOR_FLAGS 0

/* ==================== 初始化 ==================== */

int bsp_pwm_init(void)
{
	pwm_devs[PWM_HEAT]  = DEVICE_DT_GET(DT_NODELABEL(pwm2));
	pwm_devs[PWM_LL1]   = DEVICE_DT_GET(DT_NODELABEL(pwm3));
	pwm_devs[PWM_LL2]   = DEVICE_DT_GET(DT_NODELABEL(pwm3));
	pwm_devs[PWM_MOTOR] = DEVICE_DT_GET(DT_NODELABEL(pwm4));

	for (int i = 0; i < PWM_COUNT; i++) {
		if (!device_is_ready(pwm_devs[i])) {
			printk("PWM ch%d dev not ready\n", i);
			return -1;
		}
	}
	printk("PWM initialized: heat(TIM2), LEDx2(TIM3), motor(TIM4)\n");
	return 0;
}

/* ==================== 加热: TIM2 CH1 PA15 ==================== */

void bsp_pwm_set_heat(uint16_t pulse)
{
	if (pulse > PWM_HEAT_MAX_PULSE)
		pulse = PWM_HEAT_MAX_PULSE;

	uint32_t period = PWM_USEC(PWM_HEAT_PERIOD_US);
	uint32_t pulse_us = (uint32_t)((uint64_t)pulse * PWM_HEAT_PERIOD_US / PWM_HEAT_MAX_PULSE);

	pwm_set(pwm_devs[PWM_HEAT], pwm_channels[PWM_HEAT],
		period, PWM_USEC(pulse_us), HEAT_FLAGS);
}

/* ==================== LED: TIM3 CH1 PA6 / CH3 PB0 ==================== */

static void set_led_pwm(int idx, uint16_t duty)
{
	if (duty > PWM_LED_MAX_DUTY)
		duty = PWM_LED_MAX_DUTY;

	uint32_t pulse = (uint32_t)((uint64_t)duty * PWM_LED_PERIOD / PWM_LED_MAX_DUTY);

	pwm_set(pwm_devs[idx], pwm_channels[idx],
		PWM_LED_PERIOD, pulse, LED_FLAGS);
}

void bsp_pwm_set_ll1_led(uint16_t duty) { set_led_pwm(PWM_LL1, duty); }
void bsp_pwm_set_ll2_led(uint16_t duty) { set_led_pwm(PWM_LL2, duty); }

/* ==================== 步进电机: TIM4 CH2 PD13 ==================== */

/*
 * 转换公式: arr = 225000 / frequency
 * 实际频率 = freq * 1,200,000 / 225,000 = freq * 16/3
 */
void bsp_pwm_start_motor(uint16_t freq)
{
	if (freq < 30)  freq = 30;
	if (freq > 6000) freq = 6000;

	/* 周期 (ns) = 3e9 / (16 * freq) */
	uint32_t period = 3000000000UL / (16UL * (uint32_t)freq);

	pwm_set(pwm_devs[PWM_MOTOR], pwm_channels[PWM_MOTOR],
		period, period / 2, MOTOR_FLAGS);
}

void bsp_pwm_stop_motor(void)
{
	pwm_set(pwm_devs[PWM_MOTOR], pwm_channels[PWM_MOTOR],
		PWM_USEC(1000), 0, MOTOR_FLAGS);
}
