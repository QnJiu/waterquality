/*
 * hostcmd.c — 上位机命令处理
 *
 * ==================== 命令处理架构 ====================
 *
 * 命令处理流程:
 *   1. RS232CmdProcess() — 在 Modbus 任务中执行, 仅做命令派发
 *      检查 AnalyzerBeWorking 互斥标志 → post 事件唤醒 ProcessControlTask
 *   2. HostCmdProcess() — 在 ProcessControlTask 中执行, 处理所有命令
 *      根据 nHostCMD 执行对应的硬件操作, 完成后清除 nHostCMD
 *
 * ==================== 触发流程 ====================
 *
 *   上位机 Modbus 写 PDU 0 (nHostCMD)
 *   → eMBRegHoldingCB() 设置 HaveNewCMDFlag=true
 *   → Modbus 任务检测到标志 → RS232CmdProcess()
 *   → AnalyzerBeWorking 检查 → k_event_post(EV_COMM_HOST_CMD)
 *   → ProcessControlTask 检测到事件 → HostCmdProcess()
 *   → 执行命令 → 清除 nHostCMD → 清除 AnalyzerBeWorking
 *
 * ==================== 命令分类 ====================
 *
 *   系统命令:
 *     CMD_SLAVE_HALT — 安全停机(停泵+关阀+停止加热)
 *     CMD_SLAVE_RESET — 系统重启
 *     CMD_SAVE_DATA/CMD_SAVE_DEV_OPERAT_PARA — 保存参数
 *     CMD_START_TP — 启动总磷分析(设置 nAutoRun=1)
 *
 *   阀门/泵命令:
 *     CMD_START_COMBI_VALVE — 开组合阀(按位掩码)
 *     CMD_STOP_COMBI_VALVE — 关组合阀(按位掩码)
 *     CMD_PERISTALTIC_PUMP_OPT — 蠕动泵操作(速度+方向)
 *     CMD_PERISTALTIC_PUMP_STOP — 停止蠕动泵
 *
 *   温控命令:
 *     CMD_START_TEMPER_CTRL — 启动温控(占位)
 *     CMD_STOP_TEMPER_CTRL — 停止温控
 *
 *   搅拌命令:
 *     CMD_STIR_ON/CMD_STIR_OFF — 搅拌开关(占位)
 */

#include "hostcmd.h"
#include "task_modbus.h"
#include "task_process_control.h"
#include "autorun.h"
#include "bsp/bsp_valve.h"
#include "bsp/bsp_pump.h"
#include "bsp/bsp_pwm.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/reboot.h>

/* proc_events 和 EV_COMM_HOST_CMD 在 task_process_control.h 中定义 */

/* ==================== RS232CmdProcess (命令派发) ==================== */

/**
 * @brief 上位机命令派发 (Modbus 任务上下文)
 *
 * 在 Modbus 任务中执行, 持有 ModbusBusMutex。
 * 仅做命令派发: 检查互斥标志 → post 事件唤醒 ProcessControlTask。
 * 所有命令的实际执行由 HostCmdProcess() 完成。
 */
void RS232CmdProcess(void)
{
	if (!AnalyzerBeWorking) {
		AnalyzerBeWorking = true;
		k_event_post(&proc_events, EV_COMM_HOST_CMD);
	} 
}

/* ==================== HostCmdProcess (命令执行) ==================== */

/**
 * @brief 上位机命令执行 (ProcessControlTask 上下文)
 *
 * 在 ProcessControlTask 中执行, 不持有 Modbus 锁。
 * 处理所有上位机命令, 包括系统命令、阀门/泵、温控、搅拌等。
 *
 * 注意: 每个 case 执行完后都要清除 nHostCMD (设为 CMD_NONE),
 *       否则下次循环会重复执行同一命令。
 */
void HostCmdProcess(void)
{
	HOST_CMD_t cmd = (HOST_CMD_t)pMbHoldData->nHostCMD;

	switch (cmd) {
	/* ==================== 系统命令 ==================== */

	case CMD_SLAVE_HALT:
		/*
		 * 安全停机: 立即停止所有执行机构
		 *   1. 停止电机 PWM
		 *   2. 禁用泵驱动
		 *   3. 关闭所有阀门
		 *   4. 停止加热 (active-low: 60000=0%占空比)
		 */
		bsp_pwm_stop_motor();
		bsp_pump_disable();
		bsp_valve_close_all();
		bsp_pwm_set_heat(60000);
		pMbHoldData->nHostCMD = CMD_NONE;
		break;

	case CMD_SLAVE_RESET:
		/*
		 * 系统重启: 清除命令, 延时 100ms 确保 Modbus 响应发送完成
		 * 注意: 此命令执行后系统会重启, 所有未保存的数据会丢失
		 */
		pMbHoldData->nHostCMD = CMD_NONE;
		k_sleep(K_MSEC(100));
		sys_reboot(0);
		break;

	case CMD_SAVE_DATA:
	case CMD_SAVE_DEV_OPERAT_PARA:
		/* 保存参数: 参数已在寄存器中, 直接清除命令 */
		pMbHoldData->nHostCMD = CMD_NONE;
		break;

	case CMD_START_TP:
		/*
		 * 启动总磷分析: 设置 nAutoRun=1
		 * ProcessControlTask 检测到 nAutoRun>0 后调用 AutoRun()
		 */
		pMbHoldData->nAutoRun = 1;
		pMbHoldData->nHostCMD = CMD_NONE;
		break;

	case CMD_SET_BAUD:
	case CMD_SET_LOCAL_ADD:
		/* 设置 Modbus 波特率/地址: 参数已在寄存器中, 直接清除命令 */
		pMbHoldData->nHostCMD = CMD_NONE;
		break;

	/* ==================== 阀门/泵命令 ==================== */

	case CMD_START_COMBI_VALVE: {
		/*
		 * 开组合阀: 上位机通过 nCombiValveL/H 两个寄存器传递 32-bit 阀门掩码
		 *   nCombiValveL = 低 16 位 (bit0-15, 对应 Y1-Y16)
		 *   nCombiValveH = 高 16 位 (bit16-31, 对应 Y17-Y32)
		 *   例如: 打开 Y1+Y3+Y5 → mask = BIT(0)|BIT(2)|BIT(4) = 0x00000015
		 */
		uint32_t mask = pMbHoldData->nCombiValveL |
			       ((uint32_t)pMbHoldData->nCombiValveH << 16);
		bsp_valve_open_mask(mask);
		printk("HostCmd: VALVE L=%04X H=%04X mask=0x%08X\n",
		       pMbHoldData->nCombiValveL, pMbHoldData->nCombiValveH, mask);
		pMbHoldData->nHostCMD = CMD_NONE;
		break;
	}

	case CMD_STOP_COMBI_VALVE: {
		/* 关组合阀: 同开阀, 但执行关闭操作 */
		uint32_t mask = pMbHoldData->nCombiValveL |
			       ((uint32_t)pMbHoldData->nCombiValveH << 16);
		bsp_valve_close_mask(mask);
		pMbHoldData->nHostCMD = CMD_NONE;
		break;
	}

	case CMD_PERISTALTIC_PUMP_OPT:
		/*
		 * 蠕动泵操作:
		 *   nStepperMotorSpeed > 0: 正转(抽液), 速度 = nStepperMotorSpeed Hz
		 *   nStepperMotorSpeed < 0: 反转(推液), 速度 = -nStepperMotorSpeed Hz
		 *   nStepperMotorSpeed = 0: 不操作
		 *
		 * 流程:
		 *   1. 停止电机(防反转) → 延时 300ms(等待电机完全停止)
		 *   2. 设置方向 → 启动 PWM → 使能泵
		 *   3. 等待泵完成或超时(2s)
		 *   4. 超时则强制停止泵
		 */
		bsp_pwm_stop_motor();
		k_sleep(K_MSEC(300));
		bsp_pump_set_direction(pMbHoldData->nStepperMotorSpeed > 0 ?
				      MOTOR_CW : MOTOR_CCW);
		bsp_pwm_start_motor((uint16_t)(pMbHoldData->nStepperMotorSpeed > 0 ?
					       pMbHoldData->nStepperMotorSpeed :
					       -pMbHoldData->nStepperMotorSpeed));
		bsp_pump_enable();
		TimeoutFlag = false;
		if (!WaitForFlag(&TimeoutFlag, 2000, 100, &pMbHoldData->RW4230)) {
			bsp_pwm_stop_motor();
			bsp_pump_disable();
		}
		TimeoutFlag = false;
		pMbHoldData->nHostCMD = CMD_NONE;
		break;

	case CMD_PERISTALTIC_PUMP_STOP:
		/* 停止蠕动泵: 立即停止 PWM + 禁用泵驱动 */
		bsp_pwm_stop_motor();
		bsp_pump_disable();
		pMbHoldData->nHostCMD = CMD_NONE;
		break;

	/* ==================== 温控命令 ==================== */

	case CMD_START_TEMPER_CTRL:
	case CMD_START_TEMPER_CTRL_CONST:
	case CMD_START_TEMP_CTRL:
		/* 启动温控: 占位, 后续补充 PID 控温逻辑 */
		pMbHoldData->nHostCMD = CMD_NONE;
		break;

	case CMD_STOP_TEMPER_CTRL:
	case CMD_STOP_TEMP_CTRL:
		/* 停止温控: 停止加热器 PWM (active-low: 60000=不加热) */
		bsp_pwm_set_heat(60000);
		pMbHoldData->nHostCMD = CMD_NONE;
		break;

	/* ==================== 搅拌命令 ==================== */

	case CMD_STIR_ON:
	case CMD_STIR_OFF:
		/* 搅拌开关: 占位, 后续补充搅拌控制逻辑 */
		pMbHoldData->nHostCMD = CMD_NONE;
		break;

	default:
		/* 未知命令: 直接清除, 避免重复执行 */
		pMbHoldData->nHostCMD = CMD_NONE;
		break;
	}
}
