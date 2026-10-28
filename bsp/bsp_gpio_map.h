/*
 * bsp_gpio_map.h — 所有 GPIO 引脚的设备树宏映射
 *
 * 使用 Zephyr GPIO_DT_SPEC_GET 从设备树节点获取引脚定义
 *
 * 引脚分配参考: hal-reference/STM32F407VET6_引脚分配表.md
 *
 * 电平约定:
 *   阀门 Y1-Y19: active-low (LOW=开, HIGH=关), 继电器驱动
 *   LED1/LED2:   active-low, open-drain
 *   蜂鸣器:      active-high, push-pull
 *   电机 EN:     active-low (LOW=使能)
 *   电机 DIR:    active-high (HIGH=顺时针)
 */

#ifndef BSP_GPIO_MAP_H
#define BSP_GPIO_MAP_H

#include <zephyr/drivers/gpio.h>

/* 阀门 Y1-Y19 (active-low, LOW=开启阀门) */
#define VALVE_Y1  GPIO_DT_SPEC_GET(DT_NODELABEL(y1),  gpios)
#define VALVE_Y2  GPIO_DT_SPEC_GET(DT_NODELABEL(y2),  gpios)
#define VALVE_Y3  GPIO_DT_SPEC_GET(DT_NODELABEL(y3),  gpios)
#define VALVE_Y4  GPIO_DT_SPEC_GET(DT_NODELABEL(y4),  gpios)
#define VALVE_Y5  GPIO_DT_SPEC_GET(DT_NODELABEL(y5),  gpios)
#define VALVE_Y6  GPIO_DT_SPEC_GET(DT_NODELABEL(y6),  gpios)
#define VALVE_Y7  GPIO_DT_SPEC_GET(DT_NODELABEL(y7),  gpios)
#define VALVE_Y8  GPIO_DT_SPEC_GET(DT_NODELABEL(y8),  gpios)
#define VALVE_Y9  GPIO_DT_SPEC_GET(DT_NODELABEL(y9),  gpios)
#define VALVE_Y10 GPIO_DT_SPEC_GET(DT_NODELABEL(y10), gpios)
#define VALVE_Y11 GPIO_DT_SPEC_GET(DT_NODELABEL(y11), gpios)
#define VALVE_Y12 GPIO_DT_SPEC_GET(DT_NODELABEL(y12), gpios)
#define VALVE_Y13 GPIO_DT_SPEC_GET(DT_NODELABEL(y13), gpios)
#define VALVE_Y14 GPIO_DT_SPEC_GET(DT_NODELABEL(y14), gpios)
#define VALVE_Y15 GPIO_DT_SPEC_GET(DT_NODELABEL(y15), gpios)
#define VALVE_Y16 GPIO_DT_SPEC_GET(DT_NODELABEL(y16), gpios)
#define VALVE_Y17 GPIO_DT_SPEC_GET(DT_NODELABEL(y17), gpios)
#define VALVE_Y18 GPIO_DT_SPEC_GET(DT_NODELABEL(y18), gpios)
#define VALVE_Y19 GPIO_DT_SPEC_GET(DT_NODELABEL(y19), gpios)

/* LED (active-low, open-drain) */
#define LED1_GPIO GPIO_DT_SPEC_GET(DT_NODELABEL(led1), gpios)
#define LED2_GPIO GPIO_DT_SPEC_GET(DT_NODELABEL(led2), gpios)

/* 蜂鸣器 (active-high, push-pull) */
#define BUZZER_GPIO GPIO_DT_SPEC_GET(DT_NODELABEL(bzr), gpios)

/* 步进电机 (PD14 DIR: HIGH=CW, PD15 EN: LOW=enabled) */
#define MOTOR_DIR_GPIO GPIO_DT_SPEC_GET(DT_NODELABEL(motor_dir), gpios)
#define MOTOR_EN_GPIO  GPIO_DT_SPEC_GET(DT_NODELABEL(motor_en),  gpios)

#endif /* BSP_GPIO_MAP_H */
