/*
 * bsp_dac.h — DAC 输出控制
 *
 * 2 路 DAC 输出 (12-bit, 0-3.3V):
 *   DAC CH1 (PA4): 浓度 → 4-20mA 电流环输出
 *   DAC CH2 (PA5): 比色 LED 驱动电压
 */

#ifndef BSP_DAC_H
#define BSP_DAC_H

#include <stdint.h>

/* DAC 通道定义 (对应 STM32 DAC1 的两个输出通道) */
#define DAC_CH_CURRENT  1  /* PA4: 浓度电流输出 (4-20mA 转换) */
#define DAC_CH_LED      2  /* PA5: 比色 LED 驱动电压 */

#define DAC_MAX_VALUE   4095  /* 12-bit 最大值 (对应 3.3V) */

/** @brief 初始化 DAC1 设备 */
int  bsp_dac_init(void);

/**
 * @brief 设置浓度电流输出 (PA4)
 * @param value 0-4095 (0=0V, 4095=3.3V)
 */
void bsp_dac_set_current(uint16_t value);

/**
 * @brief 设置比色 LED 驱动电压 (PA5)
 * @param value 0-4095 (0=0V, 4095=3.3V)
 */
void bsp_dac_set_led(uint16_t value);

#endif /* BSP_DAC_H */
