/*
 * task_error_manage.h — 错误处理 + 安全停机
 *
 * 错误处理流程:
 *   AutoRun 模块检测到异常 → SendFunErrorCode(code)
 *   → 信号量唤醒 task_error_manage → HandleError() 安全停机
 *   → RW4230 报警码写入 (上位机可读)
 */

#ifndef TASK_ERROR_MANAGE_H
#define TASK_ERROR_MANAGE_H

#include <stdint.h>

/**
 * @brief 错误码枚举
 *
 * 由 AutoRun 各模块在检测到异常时通过 SendFunErrorCode() 发送。
 * 写入 pMbInputData->nFunErrorCode (输入寄存器, 上位机可读)。
 * HandleError() 将其映射为 RW4230 报警码。
 */
typedef enum {
	SYS_OK = 0,             /**< 无错误 */
	MEASURE_ERROR,          /**< 测量选择错误 (模块5: RW50 无效) */
	WORKEMODE_ERROR,        /**< 工作模式错误 (模块5: nStartMode 无效) */
	AD_CALIBRATION_ERROR,   /**< 液位信号异常 (模块3 AD液位标定, 模块5) */
	AD_UPLEVEL_ERROR,       /**< 上液位错误 (模块5: 液位信号未触发) */
	AD_DOWNLEVEL_ERROR,     /**< 下液位错误 (模块5: 液位信号未释放) */
	Reagent1OUT_ERROR,      /**< 试剂1排空 (模块6: 进试剂1 超时) */
	Reagent2OUT_ERROR,      /**< 试剂2排空 (模块9: 进试剂2 超时) */
	Reagent3OUT_ERROR,      /**< 试剂3排空 (模块11: 进试剂3 超时) */
	HOT_TIMEOUT_ERROR,      /**< 加热超时 (模块7: 120°C 升温超时) */
	FITLINE_ERROR,          /**< 曲线拟合错误 (模块19: k/b 参数异常) */
	REAGENT1_EMPTY,         /**< 试剂1空 (液位检测) */
	REAGENT2_EMPTY,         /**< 试剂2空 (液位检测) */
	REAGENT3_EMPTY,         /**< 试剂3空 (液位检测) */
} nErrorCode;

/* RW4230 报警码 (写入保持寄存器, 上位机可读取) */
#define RW4230_PUMP_TIMEOUT      100  /**< 泵超时 (WaitForFlag 超时) */
#define RW4230_TEMP_TIMEOUT      101  /**< 温度控制超时 (WaitForTemperature 超时) */

/**
 * @brief 发送错误码, 唤醒错误处理任务
 * @param errorcode 错误码 (SYS_OK 时忽略)
 */
void SendFunErrorCode(nErrorCode errorcode);

/**
 * @brief 错误处理任务线程
 *
 * 阻塞等待信号量, 收到后执行 HandleError() 安全停机。
 * 由 K_THREAD_DEFINE 在 main.c 中创建。
 */
void task_error_manage(void *, void *, void *);

#endif /* TASK_ERROR_MANAGE_H */
