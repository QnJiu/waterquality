/*
 * bsp_pid.h — 增量式 PID 控制算法
 *
 * 用于加热消解温度控制 (模块 7: 120°C, 模块 12: 50°C)
 *
 * 算法: 增量式 PID + 死区 + 积分限幅 + 抗积分饱和
 *   OUT = Kp * Error + Ki * ∫Error + Kd * ΔError
 *
 * 使用流程:
 *   1. ClearPidParam() 清零积分
 *   2. SetPidParam() 设置 Kp/Ki/Kd/Deadband
 *   3. SetPidMaxUk() 设置输出上限
 *   4. SetTargetValue() 设置目标温度
 *   5. 循环调用 PIDRealize(pid, 当前温度) 获取 PWM 输出值
 */

#ifndef BSP_PID_H
#define BSP_PID_H

#include <stdint.h>

/* PID 参数和状态结构体 */
typedef struct {
	float fKp;          /* 比例系数 */
	float fKi;          /* 积分系数 */
	float fKd;          /* 微分系数 */
	float fDeadband;    /* 死区 (误差小于此值时不调节) */
	float fActualValue; /* 当前实际值 (传感器读数) */
	float fTargetValue; /* 目标设定值 */
	float fError;       /* 当前误差 = Target - Actual */
	float fLastErr;     /* 上一次误差 */
	float fdError;      /* 误差变化量 = Error - LastErr */
	float fPropor;      /* 比例项输出 */
	float fIntegral;    /* 积分项累计值 */
	float fDiffer;      /* 微分项输出 */
	int   nUk;          /* 最终输出值 (PWM 占空比) */
	unsigned int nMaxUk;     /* 输出上限 (如 60000) */
	unsigned int nUkOffset;  /* 积分偏置 (防止积分从 0 启动) */
} PID_t;

/** @brief 设置目标温度 */
void SetTargetValue(PID_t *pPid, float fValue);

/** @brief 一次设置全部 PID 参数 */
void SetPidParam(PID_t *pPid, float fKp, float fKi, float fKd, float fDeadband);

/** @brief 设置比例系数 */
void SetPidKp(PID_t *pPid, float fKp);

/** @brief 设置积分系数 */
void SetPidKi(PID_t *pPid, float fKi);

/** @brief 设置微分系数 */
void SetPidKd(PID_t *pPid, float fKd);

/** @brief 设置死区范围 */
void SetPidDeadband(PID_t *pPid, float fDeadband);

/** @brief 设置输出上限 (PWM 最大占空比) */
void SetPidMaxUk(PID_t *pPid, unsigned int nMaxUk);

/** @brief 设置积分偏置 + 初始积分值 */
void SetPidUkOffset(PID_t *pPid, unsigned int nUkOffset);

/** @brief 设置积分累计值 */
void SetPidIntegral(PID_t *pPid, float fIntegral);

/** @brief 设置当前实际值 (不触发计算) */
void SetPidfActualValue(PID_t *pPid, float fActualValue);

/** @brief 无扰切换: 将当前误差设为上次误差, 防止切换时微分冲击 */
void PidBumpless(PID_t *pPid);

/** @brief 清零积分项 (重新开始温控前调用) */
void ClearPidParam(PID_t *pPid);

/**
 * @brief 执行一次 PID 计算
 * @param pPid        PID 实例
 * @param fActualValue 当前传感器读数 (如温度)
 * @return PWM 输出值 (0 ~ nMaxUk), 0=不加热, nMaxUk=最大加热
 *
 * 算法:
 *   1. Error = Target - Actual
 *   2. 若 |Error| > Deadband: 计算 P/I/D 项
 *   3. 若 |Error| <= Deadband: 仅保持积分项 (不调节)
 *   4. 输出限幅: 0 ≤ nUk ≤ nMaxUk
 */
int PIDRealize(PID_t *pPid, float fActualValue);

#endif /* BSP_PID_H */
