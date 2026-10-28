/*
 * task_process_control.c — 主分析流程控制任务
 *
 * 职责:
 *   1. 等待 AutoRun 触发 (Modbus 写 nAutoRun=1) → 调用 AutoRun() 20 模块流程
 *   2. 等待上位机命令 (EV_COMM_HOST_CMD) → 委托 HostCmdProcess() 执行
 *   3. 受控模式 (nStartMode==4) 分步执行
 *
 * 事件触发链:
 *   Modbus 写 nAutoRun → ProcessControlTask 检测 → AutoRun()
 *   Modbus 写 nHostCMD → RS232CmdProcess → k_event_post(EV_COMM_HOST_CMD)
 *   → ProcessControlTask → HostCmdProcess()
 *
 * 辅助函数:
 *   WaitForFlag()         — 带超时的标志位等待 (泵完成 ISR 设置 TimeoutFlag)
 *   WaitForTemperature()  — 带超时的温度等待 (用于 PID 控温)
 *   GetCurrentTime()      — RTC 日历读取 (当前占位, TODO: 阶段 7 补充)
 *   Set*CalibrationTime() — 校准/核查时间戳写入 MB3 寄存器
 */

#include "task_process_control.h"
#include "task_modbus.h"
#include "task_error_manage.h"
#include "autorun.h"
#include "hostcmd.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/* 事件对象: 用于 RS232CmdProcess / ModbusCoilCB → ProcessControlTask 跨任务通知 */
struct k_event proc_events;

/* ==================== 全局变量 ==================== */

volatile uint8_t g_TempCtrlFinishedFlag; /* 温控完成标志 (PID 控温模块使用) */
volatile bool TimeoutFlag;               /* 泵/操作超时标志 (ISR 或定时器设置 true) */
uint8_t AutoStep;                        /* 当前 AutoRun 步骤号 (调试用) */

/* ==================== 辅助函数 ==================== */

/**
 * @brief 等待标志位, 带超时保护
 *
 * 轮询方式等待 *pFlag 变为 true。主要用于等待泵完成:
 * 泵 ISR 完成一圈后设置 TimeoutFlag=true, 本函数检测到后返回。
 *
 * @param pFlag       要等待的标志位指针 (通常为 &TimeoutFlag)
 * @param timeout_ms  超时时间 (ms), 生产环境 PUMP_TIMEOUT_MS=20000
 * @param tick_ms     轮询间隔 (ms), 通常 100
 * @param pErrorCode  超时时写入 100 (RW4230_PUMP_TIMEOUT), 可为 NULL
 * @return true=标志位置位 (操作成功), false=超时
 */
bool WaitForFlag(volatile bool *pFlag, uint32_t timeout_ms, uint32_t tick_ms, uint16_t *pErrorCode)
{
	uint32_t elapsed = 0;
	while (!(*pFlag) && (elapsed < timeout_ms)) {
		k_sleep(K_MSEC(tick_ms));
		elapsed += tick_ms;
	}
	if (*pFlag) {
		return true;
	}
	if (pErrorCode != NULL) {
		*pErrorCode = 100;  /* MOTOR_TIMEOUT */
	}
	return false;
}

/**
 * @brief 等待温度达到目标值, 带超时保护
 *
 * 轮询方式等待温度条件满足。注意: pCurrentTemp 是指针,
 * 调用方需在每次轮询时更新温度值 (本函数不负责读取温度)。
 *
 * @param pCurrentTemp 当前温度指针 (调用方负责更新)
 * @param targetTemp   目标温度 (°C)
 * @param waitLower    true=等待温度降到目标以下 (冷却), false=等待升到目标以上 (加热)
 * @param timeout_ms   超时时间 (ms), 生产环境 TEMP_TIMEOUT_MS=800000
 * @param tick_ms      轮询间隔 (ms), 通常 TEMP_CTRL_TICK_MS=200
 * @param pErrorCode   超时时写入 101 (RW4230_TEMP_TIMEOUT), 可为 NULL
 * @return true=温度达标, false=超时
 */
bool WaitForTemperature(float *pCurrentTemp, float targetTemp, bool waitLower,
			uint32_t timeout_ms, uint32_t tick_ms, uint16_t *pErrorCode)
{
	uint32_t elapsed = 0;

	while (elapsed < timeout_ms) {
		bool condition;
		if (waitLower)
			condition = (*pCurrentTemp <= targetTemp);
		else
			condition = (*pCurrentTemp >= targetTemp);

		if (condition) return true;

		k_sleep(K_MSEC(tick_ms));
		elapsed += tick_ms;
	}

	if (pErrorCode != NULL) {
		*pErrorCode = 101;  /* TEMP_TIMEOUT */
	}
	return false;
}

/**
 * @brief 获取当前时间 (RTC 日历)
 *
 * 从 RTC 读取当前年月日时分秒。用于记录校准/核查时间戳。
 *
 * @note 当前实现返回占位值 (2026-05-25 00:00:00),
 *       Zephyr RTC calendar 受 idle timer 限制暂不可用, TODO: 阶段 7 补充
 *
 * @param pYear  年输出指针
 * @param pMonth 月输出指针
 * @param pDay   日输出指针
 * @param pHour  时输出指针
 * @param pMin   分输出指针
 * @param pSec   秒输出指针
 */
void GetCurrentTime(uint16_t *pYear, uint16_t *pMonth, uint16_t *pDay,
		    uint16_t *pHour, uint16_t *pMin, uint16_t *pSec)
{
	if (!pYear || !pMonth || !pDay || !pHour || !pMin || !pSec) return;

	/* TODO: 实现 Zephyr RTC 日历读写 (阶段 7 补充) */
	*pYear  = 2026;
	*pMonth = 5;
	*pDay   = 25;
	*pHour  = 0;
	*pMin   = 0;
	*pSec   = 0;
}

/**
 * @brief 记录零点标定时间戳到 MB3 寄存器
 *
 * 调用 GetCurrentTime() 获取当前时间, 写入 pMb3InputData 的
 * ZerocalibrationYear/Month/Day/Hours/Min/Sec 字段。
 * 上位机通过 Modbus USART3 读取这些寄存器显示最近零点标定时间。
 */
void SetZeroCalibrationTime(void)
{
	GetCurrentTime(&pMb3InputData->ZerocalibrationYear,
		       &pMb3InputData->ZerocalibrationMonth,
		       &pMb3InputData->ZerocalibrationDay,
		       &pMb3InputData->ZerocalibrationHours,
		       &pMb3InputData->ZerocalibrationMin,
		       &pMb3InputData->ZerocalibrationSec);
}

/**
 * @brief 记录量程标定时间戳到 MB3 寄存器
 *
 * 写入 RangecalibrationYear/Month/Day/Hours/Min/Sec 字段。
 */
void SetRangeCalibrationTime(void)
{
	GetCurrentTime(&pMb3InputData->RangecalibrationYear,
		       &pMb3InputData->RangecalibrationMonth,
		       &pMb3InputData->RangecalibrationDay,
		       &pMb3InputData->RangecalibrationHours,
		       &pMb3InputData->RangecalibrationMin,
		       &pMb3InputData->RangecalibrationSec);
}

/**
 * @brief 记录标液核查时间戳到 MB3 寄存器
 *
 * 写入 StandardcheckYear/Month/Day/Hours/Min/Sec 字段。
 */
void SetStandardCheckTime(void)
{
	GetCurrentTime(&pMb3InputData->StandardcheckYear,
		       &pMb3InputData->StandardcheckMonth,
		       &pMb3InputData->StandardcheckDay,
		       &pMb3InputData->StandardcheckHours,
		       &pMb3InputData->StandardcheckMin,
		       &pMb3InputData->StandardcheckSec);
}

/* ==================== ProcessControlTask 主循环 ==================== */

/**
 * @brief 主分析流程控制任务
 *
 * 事件驱动主循环, 等待并处理两类事件:
 *   1. AutoRun 触发: Modbus 写 nAutoRun (PDU 130) > 0 → 调用 AutoRun() 20 模块
 *   2. 上位机命令: RS232CmdProcess 通过 k_event_post(EV_COMM_HOST_CMD) 触发
 *      → 调用 HostCmdProcess() 执行阀门/泵/温控等操作
 *
 * 500ms 超时唤醒保证即使无事件也能周期检测 nAutoRun (Modbus 写入不触发事件时)
 *
 * 受控模式 (nStartMode==4): 上位机通过 nControlledStep 分步控制流程
 * 手动模式 (nStartMode==5): 直接执行单次测量
 */
void task_process_control(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1); ARG_UNUSED(arg2); ARG_UNUSED(arg3);

	printk("ProcessControlTask started\n");
	k_event_init(&proc_events);

	while (1) {
		/* 等待事件 (500ms 超时保证周期检测) */
		uint32_t events = k_event_wait(&proc_events,
			EV_COMM_HOST_CMD | EV_COMM_VALVE_OPT, true, K_MSEC(500));

		/* AutoRun 触发: 上位机写 nAutoRun=1 启动测量流程 */
		if (pMbHoldData->nAutoRun > 0) {
			printk("AutoRun triggered (nAutoRun=%u)\n", pMbHoldData->nAutoRun);
			pMbHoldData->nAutoRun = 0;
			AutoRun();  /* 阻塞执行 20 模块流程 */
		}

		/*
		 * 上位机命令处理
		 *
		 * 条件: 收到 EV_COMM_HOST_CMD 事件 且 nAutoRun==0
		 *   nAutoRun==0 检查的目的: AutoRun 执行期间不处理主机命令,
		 *   防止命令干扰正在进行的分析流程
		 *
		 * AnalyzerBeWorking 清除: 命令处理完成后清除, 允许接收新命令
		 *   如果因 nAutoRun>0 跳过命令处理, 也清除标志避免死锁
		 */
		if ((events & EV_COMM_HOST_CMD) && (pMbHoldData->nAutoRun == 0)) {
			pMbInputData->nDevBeRunning = 1;
			HostCmdProcess();
			AnalyzerBeWorking = false;
		} else if (events & EV_COMM_HOST_CMD) {
			/* AutoRun 进行中, 清除互斥标志避免死锁 */
			AnalyzerBeWorking = false;
		}

		/*
		 * 阀门操作: Modbus 线圈写入触发
		 *   UpdataValveState(usCoilBuf) 根据线圈位图更新阀门 GPIO
		 *   AnalyzerBeWorking = false 允许接收新的线圈写入
		 */
		if (events & EV_COMM_VALVE_OPT) {
			UpdataValveState(pMbCoilData);
			AnalyzerBeWorking = false;
		}

		/* 受控模式: 上位机分步控制 (预留) */
		if (pMbHoldData->nStartMode == 4 && pMbHoldData->nControlledStep > 0) {
			printk("Controlled mode step=%u (skeleton)\n",
			       pMbHoldData->nControlledStep);
		}
	}
}
