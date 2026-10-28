/*
 * bsp_wdt.h — IWDG 看门狗
 *
 * 状态: 阶段 8 跳过 — IWDG 与 Zephyr idle timer (RTC) 存在资源冲突,
 *       启用后板子不断复位。暂时禁用, 后续单独配置。
 *
 * TODO: 解决 IWDG 与 Zephyr tickless idle 的冲突后启用
 */

#ifndef BSP_WDT_H
#define BSP_WDT_H

/** @brief 初始化 IWDG 看门狗 (10s 超时), 当前未启用 */
int bsp_wdt_init(void);

/** @brief 喂狗 (重置看门狗计数器), 当前未启用 */
int bsp_wdt_feed(void);

#endif /* BSP_WDT_H */
