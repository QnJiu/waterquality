/*
 * task_stir_ctrl.h — 搅拌控制 + 数字输入滤波
 *
 * 提供:
 *   - 搅拌启停控制 (stir_start/stir_stop)
 *   - 数字输入消抖读取 (X1=PE6, X2=PC13)
 *   - 10ms 周期任务 (task_stir_ctrl)
 */

#ifndef TASK_STIR_CTRL_H
#define TASK_STIR_CTRL_H

#include <stdint.h>
#include <stdbool.h>

/* ==================== 搅拌控制 ==================== */

void stir_start(void);        /**< 启动搅拌 (设置 stir_flag=true) */
void stir_stop(void);         /**< 停止搅拌 (设置 stir_flag=false, 清零计数) */
bool stir_is_running(void);   /**< 查询搅拌状态 */

/* ==================== 数字输入 (经 2-bit 移位消抖) ==================== */

bool stir_get_x1(void);  /**< 读取 X1 (PE6) 消抖后的状态 */
bool stir_get_x2(void);  /**< 读取 X2 (PC13) 消抖后的状态 */

/* ==================== 任务线程 ==================== */

/**
 * @brief 搅拌控制 + 数字输入滤波任务
 *
 * 10ms 周期: 读取 X1/X2 GPIO + 消抖 + 搅拌阀门周期控制
 * 由 K_THREAD_DEFINE 在 main.c 中创建
 */
void task_stir_ctrl(void *, void *, void *);

#endif /* TASK_STIR_CTRL_H */
