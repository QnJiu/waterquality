/*
 * bsp_pwm.h — PWM 输出控制 (加热/LED/步进电机)
 *
 * 4 路 PWM 输出:
 *   TIM2 CH1 (PA15): 加热控制, active-low, ~0.58Hz 慢 PWM
 *   TIM3 CH1 (PA6):  低液位 LED 驱动, ~14kHz
 *   TIM3 CH3 (PB0):  高液位 LED 驱动, ~14kHz
 *   TIM4 CH2 (PD13): 步进电机脉冲, 频率可调 30-6000Hz
 */

#ifndef BSP_PWM_H
#define BSP_PWM_H

#include <stdint.h>
#include <stdbool.h>

/* PWM 通道枚举 */
typedef enum {
	PWM_HEAT = 0,   /* TIM2 CH1, PA15: 加热控制 (active-low) */
	PWM_LL1,        /* TIM3 CH1, PA6:  低液位 LED */
	PWM_LL2,        /* TIM3 CH3, PB0:  高液位 LED */
	PWM_MOTOR,      /* TIM4 CH2, PD13: 步进电机脉冲 */
	PWM_COUNT
} pwm_channel_t;

/* 加热 PWM 参数 (TIM2: PSC=2399, ARR=60000, ~0.583Hz, active-low) */
#define PWM_HEAT_PERIOD_US  1714000UL  /* 周期 ~1.714s = 0.583Hz */
#define PWM_HEAT_MAX_PULSE  60000      /* 最大脉宽 (对应最大加热) */

/* LED PWM 参数 (TIM3: PSC=0, ARR=6000, ~14kHz) */
#define PWM_LED_PERIOD      6000
#define PWM_LED_MAX_DUTY    6000

/** @brief 初始化 4 路 PWM 设备 (TIM2/3/4) */
int  bsp_pwm_init(void);

/**
 * @brief 设置加热 PWM 脉宽
 * @param pulse 0=不加热, 60000=最大加热 (active-low: pulse 越大, 实际高电平越短)
 * @note  AutoRun 中 bsp_pwm_set_heat(60000) 表示停止加热
 */
void bsp_pwm_set_heat(uint16_t pulse);

/** @brief 设置低液位 LED 亮度 (0=灭, 6000=最大) */
void bsp_pwm_set_ll1_led(uint16_t duty);

/** @brief 设置高液位 LED 亮度 (0=灭, 6000=最大) */
void bsp_pwm_set_ll2_led(uint16_t duty);

/**
 * @brief 启动步进电机 PWM 脉冲
 * @param freq 请求频率 30-6000Hz (实际频率 = freq * 16/3)
 * @note  仅启动脉冲, 还需 bsp_pump_enable() 使能电机
 */
void bsp_pwm_start_motor(uint16_t freq);

/** @brief 停止步进电机 PWM 脉冲 */
void bsp_pwm_stop_motor(void);

#endif /* BSP_PWM_H */
