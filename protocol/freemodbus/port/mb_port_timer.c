/*
 * mb_port_timer.c — Zephyr k_timer T3.5 字符超时处理 (3 实例)
 *
 * T3.5 超时原理:
 *   Modbus RTU 帧间隔 ≥ 3.5 字符时间。UART 接收到字节时启动 one-shot 定时器,
 *   若 3.5 字符时间内无新字节到达 → 定时器到期 → pxMBPortCBTimerExpired()
 *   → FreeModbus 认为一帧接收完毕, 开始处理。
 *
 * 超时计算: usTimeOut50us * 50 微秒
 *   9600 baud: 1 字符 ≈ 1.04ms, T3.5 ≈ 3.64ms → usTimeOut50us=73
 *   115200 baud: 1 字符 ≈ 86.8μs, T3.5 ≈ 303.8μs → usTimeOut50us=6 (最小)
 */

#include "port.h"

/* 定时器实例 + 超时值 (每路 Modbus 独立) */
static struct k_timer t35_timer1, t35_timer3, t35_timer6;
static uint32_t t35_timeout_us_1, t35_timeout_us_3, t35_timeout_us_6;

/* 定时器到期回调 — ISR 上下文, 调用 FreeModbus 帧结束处理 */
static void t35_expired_1(struct k_timer *timer) { ARG_UNUSED(timer); if (pxMBPortCBTimerExpired)  pxMBPortCBTimerExpired(); }
static void t35_expired_3(struct k_timer *timer) { ARG_UNUSED(timer); if (pxMBPortCBTimerExpired3) pxMBPortCBTimerExpired3(); }
static void t35_expired_6(struct k_timer *timer) { ARG_UNUSED(timer); if (pxMBPortCBTimerExpired6) pxMBPortCBTimerExpired6(); }

/* ==================== Instance 1 (USART1) ==================== */

/**
 * @brief 初始化 T3.5 定时器 (USART1)
 * @param usTimeOut50us 超时值 (单位 50μs), 由波特率计算: T3.5 / 50μs
 */
BOOL xMBPortTimersInit(USHORT usTimeOut50us)
{
	t35_timeout_us_1 = (uint32_t)usTimeOut50us * 50;
	k_timer_init(&t35_timer1, t35_expired_1, NULL);
	return TRUE;
}

/** @brief 启动 T3.5 one-shot 定时器 (每次接收字节时调用) */
void vMBPortTimersEnable(void)
{
	k_timer_start(&t35_timer1, K_USEC(t35_timeout_us_1), K_NO_WAIT);
}

/** @brief 停止 T3.5 定时器 (帧处理完成时调用) */
void vMBPortTimersDisable(void)
{
	k_timer_stop(&t35_timer1);
}

/* ==================== Instance 3 (USART3) ==================== */

/** @brief 初始化 T3.5 定时器 (USART3) */
BOOL xMBPortTimersInit3(USHORT usTimeOut50us)
{
	t35_timeout_us_3 = (uint32_t)usTimeOut50us * 50;
	k_timer_init(&t35_timer3, t35_expired_3, NULL);
	return TRUE;
}

/** @brief 启动 T3.5 one-shot 定时器 (USART3) */
void vMBPortTimersEnable3(void)
{
	k_timer_start(&t35_timer3, K_USEC(t35_timeout_us_3), K_NO_WAIT);
}

/** @brief 停止 T3.5 定时器 (USART3) */
void vMBPortTimersDisable3(void)
{
	k_timer_stop(&t35_timer3);
}

/* ==================== Instance 6 (USART6) ==================== */

/** @brief 初始化 T3.5 定时器 (USART6) */
BOOL xMBPortTimersInit6(USHORT usTimeOut50us)
{
	t35_timeout_us_6 = (uint32_t)usTimeOut50us * 50;
	k_timer_init(&t35_timer6, t35_expired_6, NULL);
	return TRUE;
}

/** @brief 启动 T3.5 one-shot 定时器 (USART6) */
void vMBPortTimersEnable6(void)
{
	k_timer_start(&t35_timer6, K_USEC(t35_timeout_us_6), K_NO_WAIT);
}

/** @brief 停止 T3.5 定时器 (USART6) */
void vMBPortTimersDisable6(void)
{
	k_timer_stop(&t35_timer6);
}
