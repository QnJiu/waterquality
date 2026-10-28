/*
 * task_stir_ctrl.c — 搅拌控制 + 数字输入滤波
 *
 * 功能:
 *   1. 每 10ms 读取 X1/X2 数字输入, 2-bit 移位消抖
 *      X1=PE6 (液位/门控信号1), X2=PC13 (液位/门控信号2)
 *      消抖算法: 连续 2 次采样相同才更新状态
 *   2. 搅拌控制: FlagStir=true 时按 nStirWorkTime/nStirSleepTime 周期开关阀门
 *      阀门号由 pMbHoldData->nStirValve 指定 (1-indexed, 默认 Y18)
 *      周期: 先休眠 nStirSleepTime*10ms, 再工作 nStirWorkTime*10ms
 */

#include "task_stir_ctrl.h"
#include "task_modbus.h"
#include "bsp/bsp_valve.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/* 数字输入 GPIO 定义 */
static const struct gpio_dt_spec x1_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(x1), gpios); /* PE6 */
static const struct gpio_dt_spec x2_spec = GPIO_DT_SPEC_GET(DT_NODELABEL(x2), gpios); /* PC13 */

/* 消抖滤波状态: 2-bit 移位寄存器, 连续 2 次相同才更新 */
static uint16_t io1_filter;  /* X1 移位寄存器 */
static uint16_t io2_filter;  /* X2 移位寄存器 */
static bool x1_state;        /* X1 消抖后的稳定状态 */
static bool x2_state;        /* X2 消抖后的稳定状态 */

/* 搅拌控制状态 */
static bool stir_flag;       /* 搅拌使能标志 (stir_start/stop 控制) */
static uint32_t stir_count;  /* 当前周期计数 (ms) */

void stir_start(void)  { stir_flag = true; }
void stir_stop(void)   { stir_flag = false; stir_count = 0; }
bool stir_is_running(void) { return stir_flag; }

bool stir_get_x1(void) { return x1_state; }
bool stir_get_x2(void) { return x2_state; }

/**
 * @brief 打开搅拌阀门
 *
 * 从 pMbHoldData->nStirValve 读取阀门号 (1-indexed), 范围 1-19。
 * 若值无效则重置为默认 18 (Y18) 并打开。
 */
static void open_stir(void)
{
	uint16_t valve = pMbHoldData->nStirValve;
	if (valve > 19 || valve < 1) {
		valve = 18;  /* 默认 Y18 */
		pMbHoldData->nStirValve = valve;
	}
	bsp_valve_open(valve);
}

/**
 * @brief 关闭搅拌阀门
 *
 * 同 open_stir(), 但执行关闭操作。
 */
static void close_stir(void)
{
	uint16_t valve = pMbHoldData->nStirValve;
	if (valve > 19 || valve < 1) {
		valve = 18;  /* 默认 Y18 */
		pMbHoldData->nStirValve = valve;
	}
	bsp_valve_close(valve);
}

/**
 * @brief 搅拌控制 + 数字输入滤波任务
 *
 * 10ms 周期主循环, 两个功能:
 *
 * 1. 数字输入消抖:
 *    X1 (PE6) 和 X2 (PC13) 使用 2-bit 移位寄存器消抖。
 *    连续 2 次采样为 1 → x_state=true, 连续 2 次为 0 → x_state=false。
 *    采样值不一致时保持原状态 (消抖窗口 10ms)。
 *
 * 2. 搅拌周期控制:
 *    stir_flag=true 时, 按以下周期搅拌:
 *    ┌─────────────┬─────────────┐
 *    │  休眠阶段   │  工作阶段   │
 *    │ sleep_ms    │ work_ms     │
 *    │ 阀门关闭    │ 阀门打开    │
 *    └─────────────┴─────────────┘
 *    寄存器: nStirSleepTime (单位 10ms), nStirWorkTime (单位 10ms)
 *    阀门号: nStirValve (1-indexed, 默认 Y18)
 */
void task_stir_ctrl(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1); ARG_UNUSED(arg2); ARG_UNUSED(arg3);

	/* 配置数字输入 GPIO */
	gpio_pin_configure_dt(&x1_spec, GPIO_INPUT);
	gpio_pin_configure_dt(&x2_spec, GPIO_INPUT);

	printk("StirCtrlTask started (X1=PE6, X2=PC13, stir=Y%u)\n",
	       pMbHoldData->nStirValve ? pMbHoldData->nStirValve : 18);

	while (1) {
		k_sleep(K_MSEC(10));  /* 10ms 采样周期 */

		/* X1 消抖: 2-bit 移位寄存器, bit[1:0] 连续相同才更新 */
		io1_filter <<= 1;
		io1_filter |= gpio_pin_get_dt(&x1_spec);
		if ((io1_filter & 0x03) == 0x03)
			x1_state = true;
		else if ((io1_filter & 0x03) == 0x00)
			x1_state = false;
		/* 0x01/0x02 时不更新 (消抖窗口内) */

		/* X2 消抖: 同 X1 */
		io2_filter <<= 1;
		io2_filter |= gpio_pin_get_dt(&x2_spec);
		if ((io2_filter & 0x03) == 0x03)
			x2_state = true;
		else if ((io2_filter & 0x03) == 0x00)
			x2_state = false;

		/* 搅拌周期控制 */
		if (stir_flag) {
			uint32_t work_ms = pMbHoldData->nStirWorkTime * 10U;
			uint32_t sleep_ms = pMbHoldData->nStirSleepTime * 10U;
			uint32_t cycle_ms = work_ms + sleep_ms;

			if (cycle_ms == 0) {
				close_stir();
				continue;
			}

			stir_count += 10;
			if (stir_count < sleep_ms)
				close_stir();   /* 休眠阶段: 关阀 */
			else
				open_stir();    /* 工作阶段: 开阀 */

			/* 周期结束: 关阀 + 计数清零 */
			if (stir_count >= cycle_ms) {
				stir_count = 0;
				close_stir();
			}
		} else {
			close_stir();  /* 搅拌未使能: 始终关阀 */
		}
	}
}
