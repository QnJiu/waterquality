/*
 * bsp_rtc.h — RTC 时间功能
 *
 * 限制: Zephyr 已将 STM32 RTC 硬件用作系统 idle timer (tickless idle),
 *       不能同时用于日历读写。目前仅通过 k_uptime_get_32() 获取系统运行时间。
 *
 * TODO: 实现 RTC 日历读写 (需要修改 Zephyr Kconfig 或使用 backup domain)
 */

#ifndef BSP_RTC_H
#define BSP_RTC_H

#include <stdint.h>

/** @brief RTC 初始化 (当前为空, RTC 由 Zephyr 系统自动管理) */
int  bsp_rtc_init(void);

/**
 * @brief 获取系统运行时间 (毫秒)
 * @return 32-bit 毫秒计数器 (约 49.7 天溢出)
 * @note  底层使用 Zephyr k_uptime_get_32(), 基于 RTC 硬件计时
 */
uint32_t bsp_rtc_get_ticks(void);

#endif /* BSP_RTC_H */
