/*
 * task_process_control.h — 主分析流程控制任务 + 辅助函数
 *
 * 提供:
 *   - ProcessControlTask 主循环 (AutoRun 触发 + 上位机命令分发)
 *   - WaitForFlag/WaitForTemperature 超时等待 (供 autorun.c 调用)
 *   - GetCurrentTime + 校准时间戳 (供模块 19/20 调用)
 */

#ifndef TASK_PROCESS_CONTROL_H
#define TASK_PROCESS_CONTROL_H

#include <stdint.h>
#include <stdbool.h>

/* ==================== 超时等待函数 ==================== */

/**
 * @brief 等待标志位, 带超时保护
 * @param pFlag       要等待的标志位指针 (通常 &TimeoutFlag)
 * @param timeout_ms  超时时间 (ms)
 * @param tick_ms     轮询间隔 (ms)
 * @param pErrorCode  超时时写入 100, 可为 NULL
 * @return true=标志位置位, false=超时
 */
bool WaitForFlag(volatile bool *pFlag, uint32_t timeout_ms, uint32_t tick_ms, uint16_t *pErrorCode);

/**
 * @brief 等待温度达到目标值, 带超时保护
 * @param pCurrentTemp 当前温度指针 (调用方负责更新)
 * @param targetTemp   目标温度 (°C)
 * @param waitLower    true=等待降到目标以下 (冷却), false=等待升到目标以上 (加热)
 * @param timeout_ms   超时时间 (ms)
 * @param tick_ms      轮询间隔 (ms)
 * @param pErrorCode   超时时写入 101, 可为 NULL
 * @return true=温度达标, false=超时
 */
bool WaitForTemperature(float *pCurrentTemp, float targetTemp, bool waitLower,
			uint32_t timeout_ms, uint32_t tick_ms, uint16_t *pErrorCode);

/**
 * @brief 获取当前时间 (RTC 日历)
 * @param pYear/pMonth/pDay/pHour/pMin/pSec 输出指针
 * @note 当前返回占位值, TODO: Zephyr RTC calendar 实现后替换
 */
void GetCurrentTime(uint16_t *pYear, uint16_t *pMonth, uint16_t *pDay,
		    uint16_t *pHour, uint16_t *pMin, uint16_t *pSec);

/* ==================== 校准时间戳 (写入 MB3 寄存器) ==================== */

void SetZeroCalibrationTime(void);    /**< 记录零点标定时间 */
void SetRangeCalibrationTime(void);   /**< 记录量程标定时间 */
void SetStandardCheckTime(void);      /**< 记录标液核查时间 */

/* ==================== 任务线程 ==================== */

/**
 * @brief 主分析流程控制任务
 *
 * 事件驱动循环: 等待 AutoRun 触发 / 上位机命令 / 受控模式步骤
 * 由 K_THREAD_DEFINE 在 main.c 中创建
 */
void task_process_control(void *arg1, void *arg2, void *arg3);

/* ==================== 事件定义 ==================== */

#include <zephyr/kernel.h>

#define EV_COMM_HOST_CMD  1  /* 上位机命令 (RS232CmdProcess default 分支触发) */
#define EV_COMM_VALVE_OPT 2  /* 阀门操作 (Modbus 线圈写入触发) */

extern struct k_event proc_events;

/* ==================== 全局标志 ==================== */

extern volatile uint8_t g_TempCtrlFinishedFlag; /**< 温控完成标志 (PID 控温模块设置) */
extern volatile bool TimeoutFlag;                /**< 超时标志 (ISR/定时器设置 true) */
extern uint8_t AutoStep;                         /**< 当前 AutoRun 步骤号 (调试用) */

#endif /* TASK_PROCESS_CONTROL_H */
