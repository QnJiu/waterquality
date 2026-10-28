/*
 * task_error_manage.c — 错误处理 + 安全停机
 *
 * 架构:
 *   SendFunErrorCode() → k_sem_give → task_error_manage → HandleError()
 *   HandleError() 执行安全停机: 停泵、关阀、停止加热
 */

#include "task_error_manage.h"
#include "task_modbus.h"
#include "bsp/bsp_valve.h"
#include "bsp/bsp_pump.h"
#include "bsp/bsp_pwm.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/* 错误信号量: SendFunErrorCode() give → task_error_manage take */
static struct k_sem error_sem;

/**
 * @brief 安全停机 + 报警码映射
 *
 * 收到错误码后执行安全停机序列:
 *   1. 停止电机 (bsp_pwm_stop_motor)
 *   2. 禁用泵 (bsp_pump_disable)
 *   3. 关闭所有阀门 (bsp_valve_close_all)
 *   4. 停止加热 (bsp_pwm_set_heat(60000), active-low)
 *
 * 然后将错误码映射为 RW4230 报警码 (上位机可读):
 *   40=泵超时, 41=温度控制, 42=液位, 43=试剂, 44=测量, 99=未知
 *
 * @param code 错误码 (nErrorCode 枚举)
 */
static void HandleError(nErrorCode code)
{
	printk("HandleError: code=%d\n", (int)code);

	/* 安全停机 */
	bsp_pwm_stop_motor();
	bsp_pump_disable();
	bsp_valve_close_all();
	bsp_pwm_set_heat(60000);  /* 停止加热 (最大占空比=不加热) */

	/* 报警码写入 RW4230 */
	switch (code) {
	case Reagent1OUT_ERROR:
	case REAGENT1_EMPTY:
	case Reagent2OUT_ERROR:
	case REAGENT2_EMPTY:
	case Reagent3OUT_ERROR:
	case REAGENT3_EMPTY:
		pMbHoldData->RW4230 = 43;
		break;
	case HOT_TIMEOUT_ERROR:
		pMbHoldData->RW4230 = 41;
		break;
	case AD_CALIBRATION_ERROR:
	case AD_UPLEVEL_ERROR:
	case AD_DOWNLEVEL_ERROR:
		pMbHoldData->RW4230 = 42;
		break;
	case MEASURE_ERROR:
	case FITLINE_ERROR:
		pMbHoldData->RW4230 = 44;
		break;
	default:
		pMbHoldData->RW4230 = 99;
		break;
	}

	printk("  Safety halt done, RW4230=%u\n", pMbHoldData->RW4230);
}

/**
 * @brief 发送错误码, 唤醒错误处理任务
 *
 * 将错误码写入 pMbInputData->nFunErrorCode (输入寄存器, 上位机可读),
 * 然后 give 信号量唤醒 task_error_manage 执行 HandleError()。
 *
 * @param errorcode 错误码 (SYS_OK 时忽略不处理)
 */
void SendFunErrorCode(nErrorCode errorcode)
{
	if (errorcode == SYS_OK) return;

	pMbInputData->nFunErrorCode = errorcode;
	k_sem_give(&error_sem);
}

/**
 * @brief 错误处理任务线程
 *
 * 阻塞等待 error_sem 信号量, 收到后调用 HandleError() 执行安全停机。
 * 由 K_THREAD_DEFINE 在 main.c 中创建, 优先级高于业务任务。
 */
void task_error_manage(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1); ARG_UNUSED(arg2); ARG_UNUSED(arg3);

	k_sem_init(&error_sem, 0, 1);
	printk("ErrorCodeManageTask started\n");

	while (1) {
		if (k_sem_take(&error_sem, K_FOREVER) == 0) {
			HandleError((nErrorCode)pMbInputData->nFunErrorCode);
		}
	}
}
