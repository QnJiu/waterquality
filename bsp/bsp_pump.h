/*
 * bsp_pump.h — 步进电机 / 蠕动泵控制 API
 *
 * 硬件:
 *   - PD14 (DIR): 电机方向, active-high (HIGH=顺时针/抽, LOW=逆时针/推)
 *   - PD15 (EN):  电机使能, active-low (LOW=使能, HIGH=禁用)
 *   - PD13 (PWM): 步进电机脉冲, 由 TIM4 CH2 驱动 (bsp_pwm.c)
 *
 * 泵速由 PWM 频率决定, bsp_pwm_start_motor(speed) 设置脉冲频率
 */

#ifndef BSP_PUMP_H
#define BSP_PUMP_H

#include <stdbool.h>

/* 电机方向 (对应 DIR 引脚电平) */
#define MOTOR_CW   true   /* 顺时针 (抽液) */
#define MOTOR_CCW  false  /* 逆时针 (推液) */

/** @brief 初始化电机 GPIO (DIR + EN), 初始状态: CCW + 禁用 */
int  bsp_pump_init(void);

/**
 * @brief 设置电机方向
 * @param cw true=顺时针(MOTOR_CW), false=逆时针(MOTOR_CCW)
 */
void bsp_pump_set_direction(bool cw);

/** @brief 使能电机 (EN=LOW), 需先调用 bsp_pwm_start_motor() 启动脉冲 */
void bsp_pump_enable(void);

/** @brief 禁用电机 (EN=HIGH), 停止转动但不关 PWM */
void bsp_pump_disable(void);

#endif /* BSP_PUMP_H */
