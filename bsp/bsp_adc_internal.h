/*
 * bsp_adc_internal.h — 内部 ADC1 驱动 (6 通道, 32 次累加平均)
 *
 * 通道分配:
 *   CH0  (PA0): 消解池温度传感器 (NTC)
 *   CH1  (PA1): 比色皿温度传感器 (NTC)
 *   CH7  (PA7): 低液位检测 LED (LL1)
 *   CH9  (PB1): 高液位检测 LED (LL2)
 *   CH16 (内部): 芯片温度传感器 (用于 PID 温控代理)
 *   CH18 (内部): VBAT 电池电压 (1/2 分压)
 *
 * 采集方式: 线程周期调用 bsp_adc_internal_sample(), 累加 32 次后取平均
 *
 * 注意: 内部温度/VBAT 需手动使能 ADC123_COMMON->CCR 的 TSVREFE/VBATE 位
 *       (Zephyr ADC 驱动不自动设置)
 */

#ifndef BSP_ADC_INTERNAL_H
#define BSP_ADC_INTERNAL_H

#include <stdint.h>
#include <stdbool.h>

/* ADC 通道 ID */
#define ADC_CH_POOL_TEMP   0   /* PA0, 消解池温度 */
#define ADC_CH_CAVE_TEMP   1   /* PA1, 比色皿温度 */
#define ADC_CH_LL1         7   /* PA7, 低液位 LED */
#define ADC_CH_LL2         9   /* PB1, 高液位 LED */
#define ADC_CH_VTEMP       16  /* 内部温度传感器 */
#define ADC_CH_VBAT        18  /* VBAT 电池电压 */

#define ADC_CHANNEL_COUNT  6
#define ADC_SAMPLE_COUNT   32  /* 累加 32 次后取平均 */

/* 累加器 (32 位宽, 足够存 32 次 12-bit 读数的和) */
typedef struct {
	uint32_t pool_temp;   /* PA0 */
	uint32_t cave_temp;   /* PA1 */
	uint32_t ll1;         /* PA7 */
	uint32_t ll2;         /* PB1 */
	uint32_t vtemp;       /* 内部温度 */
	uint32_t vbat;        /* VBAT */
} adc_accum_t;

/* 平均后的原始 ADC 值 */
typedef struct {
	uint16_t pool_temp;
	uint16_t cave_temp;
	uint16_t ll1;
	uint16_t ll2;
	uint16_t vtemp;
	uint16_t vbat;
} adc_average_t;

/* 物理量 */
typedef struct {
	float cpu_temp;   /* 芯片温度 (Celsius) */
	float vbat_volt;  /* 电池电压 (V) */
	float pool_temp;  /* 消解池温度 (待转换) */
	float cave_temp;  /* 比色皿温度 (待转换) */
} adc_physical_t;

/** @brief 初始化 ADC1, 配置 6 通道, 使能内部温度/VBAT, 初始化互斥锁 */
int  bsp_adc_internal_init(void);

/**
 * @brief 执行一次 6 通道扫描并累加到内部累加器
 * @return 0=成功, -1=ADC 读取失败
 * @note 调用 ADC_SAMPLE_COUNT (32) 次后自动计算平均值
 */
int  bsp_adc_internal_sample(void);

/**
 * @brief 获取最新平均值 (线程安全)
 * @param avg 输出: 6 通道的平均 ADC 原始值
 */
void bsp_adc_internal_read_average(adc_average_t *avg);

/**
 * @brief 将 ADC 原始值转换为物理量
 * @param avg  输入: 平均 ADC 值
 * @param phys 输出: cpu_temp(°C), vbat_volt(V), pool_temp, cave_temp
 *
 * 转换公式:
 *   CPU 温度: (V_sense - 0.76) / 0.0025 + 25 (STM32F4 数据手册)
 *   VBAT:     code * 2 * 3.3 / 4095 (内部 1/2 分压)
 *   外部通道: 仅保存原始值, 物理量转换由上位机完成
 */
void bsp_adc_internal_convert(const adc_average_t *avg, adc_physical_t *phys);

#endif /* BSP_ADC_INTERNAL_H */
