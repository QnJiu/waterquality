/*
 * autorun.h — AutoRun 20 模块分析流程
 *
 * 总磷水质分析仪核心测量流程, 由 ProcessControlTask 触发
 *
 * 触发方式: Modbus 写 nAutoRun=1 (PDU 130, 0x82)
 *
 * 流程概要:
 *   排空 → 润洗 → 液位校准 → 进样 → 进试剂 → 加热消解 120°C
 *   → 进蒸馏水/试剂 → 浊度补偿 → 静置反应 50°C → 光谱读数
 *   → 吸光度计算 → 浓度计算 → 模式判断
 *
 * 模块函数表: modules[] 数组, 每个模块返回下一模块编号, 返回 0 退出
 */

#ifndef AUTORUN_H
#define AUTORUN_H

#include <stdint.h>
#include <stdbool.h>

/* 模块编号 (用于 modules[] 数组索引和 RW4229 步骤上报) */
#define MOD_DRAIN            1   /* 排空管路 + 排空比色皿 */
#define MOD_ZERO_RINSE       2   /* 零标流程 / 润洗 (RW50 判断跳转) */
#define MOD_ZERO_LEVEL_CAL   3   /* 零点标定液位校准 (最多重试 5 次) */
#define MOD_EXT_PUMP         4   /* 外接泵循环 */
#define MOD_SAMPLING         5   /* 进样 (根据 RW50 选择源阀) */
#define MOD_REAGENT1         6   /* 进试剂 1 (Y3 空气阀) */
#define MOD_HEAT_DIGEST      7   /* 加热消解 120°C, 保持 5 分钟 */
#define MOD_DISTILLED_WATER  8   /* 进蒸馏水 (Y5) */
#define MOD_REAGENT2         9   /* 进试剂 2 (Y6) */
#define MOD_TURBIDITY        10  /* 浊度补偿 V3 (ADS1220 10 次采样) */
#define MOD_REAGENT3         11  /* 进试剂 3 (Y7) */
#define MOD_SETTLE_REACT     12  /* 静置反应 50°C, 保持 5 分钟 */
#define MOD_READ_V2          13  /* 出 V2 值 (反应后光谱读数) */
#define MOD_DRAIN_CLEAN      14  /* 排液清洗 */
#define MOD_READ_V1_BLANK    15  /* 出 V1 空白值 (蒸馏水基准) */
#define MOD_READ_V1_REACT    16  /* 出 V1 反应值 (最终读数) */
#define MOD_ABSORBANCE       17  /* 吸光度计算 log10(V1/V2) + 浊度补偿 */
#define MOD_FINISH           18  /* 流程结束, 关闭全部外设 */
#define MOD_CALC_RESULT      19  /* 浓度计算 (k*abs+b) + 写入 MB3 */
#define MOD_MODE_JUDGE       20  /* 模式判断 (根据 RW50 设置完成标志) */
#define MOD_SAMPLING_RETURN  24  /* 进样后返回 */
#define MOD_SAMPLER_WAIT     30  /* 采样器信号等待 */

/* 模块函数类型: 返回下一模块编号, 返回 0 退出 AutoRun */
typedef uint8_t (*autorun_module_fn)(void);

/**
 * @brief AutoRun 主循环 (由 ProcessControlTask 调用)
 *
 * 顺序执行 modules[] 函数表, 每个模块返回下一模块编号
 * 在 RW4229 中上报当前模块号, 1000 步后强制退出
 */
void AutoRun(void);

#endif /* AUTORUN_H */
