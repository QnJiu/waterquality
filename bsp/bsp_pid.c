/*
 * bsp_pid.c — 增量式 PID 控制算法实现
 *
 * 用途: 加热消解温度控制
 *   模块 7: 控温 120°C (Kp=3600, Ki=8, Kd=800)
 *   模块 12: 控温 50°C (Kp=3800, Ki=6, Kd=800)
 *   PWM 输出: 0=不加热, 60000=最大加热 (active-low, 实际取反)
 */

#include <math.h>
#include "bsp/bsp_pid.h"

/* ==================== 参数设置函数 ==================== */

void SetTargetValue(PID_t *pPid, float fValue)
{
	pPid->fTargetValue = fValue;
}

void SetPidParam(PID_t *pPid, float fKp, float fKi, float fKd, float fDeadband)
{
	pPid->fKp = fKp;
	pPid->fKi = fKi;
	pPid->fKd = fKd;
	pPid->fDeadband = fDeadband;
}

void SetPidKp(PID_t *pPid, float fKp)        { pPid->fKp = fKp; }
void SetPidKi(PID_t *pPid, float fKi)        { pPid->fKi = fKi; }
void SetPidKd(PID_t *pPid, float fKd)        { pPid->fKd = fKd; }
void SetPidDeadband(PID_t *pPid, float fDeadband) { pPid->fDeadband = fDeadband; }

void SetPidMaxUk(PID_t *pPid, unsigned int nMaxUk)
{
	pPid->nMaxUk = nMaxUk;
}

void SetPidUkOffset(PID_t *pPid, unsigned int nUkOffset)
{
	pPid->nUkOffset = nUkOffset;
	pPid->fIntegral = pPid->nUkOffset;
}

void SetPidIntegral(PID_t *pPid, float fIntegral)
{
	pPid->fIntegral = fIntegral;
}

void SetPidfActualValue(PID_t *pPid, float fActualValue)
{
	pPid->fActualValue = fActualValue;
}

/* 无扰切换: 将当前误差设为上次误差, 防止模式切换时微分项产生冲击 */
void PidBumpless(PID_t *pPid)
{
	pPid->fLastErr = pPid->fTargetValue - pPid->fActualValue;
}

/* 清零积分项 (重新开始温控前调用) */
void ClearPidParam(PID_t *pPid)
{
	pPid->fIntegral = 0;
}

/* ==================== PID 核心计算 ==================== */

/*
 * 增量式 PID 计算
 *
 * 输入: fActualValue = 当前传感器读数 (如温度值)
 * 输出: nUk = PWM 占空比 (0 ~ nMaxUk)
 *
 * 算法流程:
 *   1. 计算误差 Error = Target - Actual
 *   2. 若 |Error| > Deadband (需要调节):
 *      - P 项 = Kp * Error
 *      - I 项 += Ki * Error (带限幅, 防积分饱和)
 *      - D 项 = Kd * (Error - LastError)
 *      - 输出 = P + I + D
 *   3. 若 |Error| <= Deadband (在死区内):
 *      - 输出 = 积分项 (保持当前状态)
 *   4. 输出限幅: 0 ≤ nUk ≤ nMaxUk
 */
int PIDRealize(PID_t *pPid, float fActualValue)
{
	pPid->fActualValue = fActualValue;
	pPid->fError = pPid->fTargetValue - pPid->fActualValue;

	if (fabs(pPid->fError) > pPid->fDeadband) {
		/* 需要调节: 计算 P/I/D 三项 */

		/* P 项: 比例输出 */
		pPid->fPropor = pPid->fError * pPid->fKp;

		if (fabs(pPid->fPropor) > pPid->nMaxUk) {
			/* 比例项超限: 积分项重置为偏置值, 防止积分饱和 */
			pPid->fIntegral = pPid->nUkOffset;
		} else {
			/* I 项: 积分累加 */
			pPid->fIntegral += pPid->fKi * pPid->fError;

			if (pPid->fIntegral > pPid->nMaxUk) {
				/* 积分上限 */
				pPid->fIntegral = pPid->nMaxUk;
			} else if (pPid->fIntegral >= 0 && pPid->fIntegral < pPid->nUkOffset) {
				/* 积分在 0 和偏置之间: 更新偏置 */
				pPid->nUkOffset = pPid->fIntegral;
			} else if (pPid->fIntegral < 0) {
				/* 积分不允许为负 (加热系统) */
				pPid->fIntegral = 0;
			}
		}

		/* D 项: 微分输出 */
		pPid->fdError = pPid->fError - pPid->fLastErr;
		pPid->fDiffer = pPid->fdError * pPid->fKd;

		/* 总输出 = P + I + D (四舍五入) */
		pPid->nUk = (int)(pPid->fPropor + pPid->fIntegral + pPid->fDiffer + 0.5f);
	} else {
		/* 在死区内: 仅保持积分项 (四舍五入) */
		pPid->nUk = (pPid->fIntegral + 0.5f);
	}

	/* 保存本次误差供下次微分计算使用 */
	pPid->fLastErr = pPid->fError;

	/* 输出限幅: 0 ≤ nUk ≤ nMaxUk */
	if (pPid->nUk < 0)
		pPid->nUk = 0;
	if ((pPid->nMaxUk > 0) && (pPid->nUk > pPid->nMaxUk))
		pPid->nUk = pPid->nMaxUk;

	return pPid->nUk;
}
