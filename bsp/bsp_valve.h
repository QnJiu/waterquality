/*
 * bsp_valve.h — 阀门/继电器控制 API
 *
 * 19 路阀门通过继电器控制，active-low 驱动 (LOW=开, HIGH=关)
 *
 * 阀门物理功能:
 *   Y1  — 进水阀       Y2  — 水样阀       Y3  — 空气阀/试剂1
 *   Y4  — 比色阀       Y5  — 蒸馏水阀     Y6  — 试剂2阀
 *   Y7  — 试剂3阀      Y8  — 量程液阀     Y9  — 消解下阀
 *   Y10 — 消解上阀      Y11 — 冷却风扇     Y12 — 备用
 *   Y13 — 废液阀       Y14 — 核查液阀     Y15-Y19 — 备用
 *
 * 位掩码约定 (open_mask/close_mask):
 *   bit 0 = Y1, bit 1 = Y2, ... bit 18 = Y19
 *   即 bit N = valve (N+1)
 */

#ifndef BSP_VALVE_H
#define BSP_VALVE_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>

/* 阀门编号枚举 (1-indexed, 0 保留) */
typedef enum {
	VALVE_1 = 1,  VALVE_2,  VALVE_3,  VALVE_4,  VALVE_5,
	VALVE_6,      VALVE_7,  VALVE_8,  VALVE_9,  VALVE_10,
	VALVE_11,     VALVE_12, VALVE_13, VALVE_14, VALVE_15,
	VALVE_16,     VALVE_17, VALVE_18, VALVE_19,
	VALVE_COUNT = 20  /* 数组大小 (含索引 0 保留) */
} valve_name_t;

#define VALVE_ALL ((uint32_t)0xFFFFFFFF)  /* 全部阀门掩码 */

/**
 * @brief 初始化全部 19 路阀门 GPIO (输出模式, 初始关闭)
 * @return 0=成功, -1=GPIO 设备未就绪
 */
int  bsp_valve_init(void);

/**
 * @brief 开启单个阀门 (active-low: 输出 LOW)
 * @param valve 阀门编号 1-19
 */
void bsp_valve_open(valve_name_t valve);

/**
 * @brief 关闭单个阀门 (active-low: 输出 HIGH)
 * @param valve 阀门编号 1-19
 */
void bsp_valve_close(valve_name_t valve);

/** @brief 开启全部阀门 */
void bsp_valve_open_all(void);

/** @brief 关闭全部阀门 */
void bsp_valve_close_all(void);

/**
 * @brief 按位掩码开启阀门
 * @param mask bit N = valve (N+1), 例如 bit0=Y1, bit12=Y13
 */
void bsp_valve_open_mask(uint32_t mask);

/**
 * @brief 按位掩码关闭阀门
 * @param mask 同 bsp_valve_open_mask
 */
void bsp_valve_close_mask(uint32_t mask);

#endif /* BSP_VALVE_H */
