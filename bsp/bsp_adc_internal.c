/*
 * bsp_adc_internal.c — 内部 ADC1 驱动 (6 通道, 32 次累加平均)
 */

#include "bsp/bsp_adc_internal.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/sys/printk.h>
#include <string.h>

/* STM32F4 内部温度传感器和 VBAT 通道使能 (Zephyr 驱动不自动设置) */
#include <stm32f4xx_ll_adc.h>

/* ADC 设备 */
#define ADC_NODE DT_NODELABEL(adc1)
static const struct device *adc_dev = DEVICE_DT_GET(ADC_NODE);

/* 通道配置 */
static const struct adc_channel_cfg ch_cfgs[] = {
	[0] = { .channel_id = ADC_CH_POOL_TEMP, .gain = ADC_GAIN_1, .reference = ADC_REF_INTERNAL, .acquisition_time = ADC_ACQ_TIME_MAX },
	[1] = { .channel_id = ADC_CH_CAVE_TEMP, .gain = ADC_GAIN_1, .reference = ADC_REF_INTERNAL, .acquisition_time = ADC_ACQ_TIME_MAX },
	[2] = { .channel_id = ADC_CH_LL1,       .gain = ADC_GAIN_1, .reference = ADC_REF_INTERNAL, .acquisition_time = ADC_ACQ_TIME_MAX },
	[3] = { .channel_id = ADC_CH_LL2,       .gain = ADC_GAIN_1, .reference = ADC_REF_INTERNAL, .acquisition_time = ADC_ACQ_TIME_MAX },
	[4] = { .channel_id = ADC_CH_VTEMP,       .gain = ADC_GAIN_1, .reference = ADC_REF_INTERNAL, .acquisition_time = ADC_ACQ_TIME_MAX },
	[5] = { .channel_id = ADC_CH_VBAT,        .gain = ADC_GAIN_1, .reference = ADC_REF_INTERNAL, .acquisition_time = ADC_ACQ_TIME_MAX },
};

/* 通道排序: 按通道号升序 (Zephyr 以此顺序填充 buffer) */
static const uint8_t ch_order[] = { ADC_CH_POOL_TEMP, ADC_CH_CAVE_TEMP, ADC_CH_LL1, ADC_CH_LL2, ADC_CH_VTEMP, ADC_CH_VBAT };

/* 累加器和最新平均值 */
static adc_accum_t   accumulator;
static adc_average_t latest_average;
static uint32_t      sample_count;
static struct k_mutex adc_mutex;

/* ==================== 初始化 ==================== */

int bsp_adc_internal_init(void)
{
	if (!device_is_ready(adc_dev)) {
		printk("ADC1 not ready\n");
		return -1;
	}

	/* 配置通道 */
	for (int i = 0; i < ADC_CHANNEL_COUNT; i++) {
		int ret = adc_channel_setup(adc_dev, &ch_cfgs[i]);
		if (ret < 0) {
			printk("ADC ch%d setup failed: %d\n", ch_cfgs[i].channel_id, ret);
			return -1;
		}
	}

	k_mutex_init(&adc_mutex);
	memset(&accumulator, 0, sizeof(accumulator));
	sample_count = 0;

	/* 使能内部温度传感器和 VBAT 通道 (Zephyr 驱动不自动设置 TSVREFE/VBATE) */
	ADC123_COMMON->CCR |= (ADC_CCR_TSVREFE | ADC_CCR_VBATE);

	printk("ADC1 initialized: 6 channels, 12-bit\n");
	return 0;
}

/* ==================== 单次采样 (读 6 通道) ==================== */

static int adc_sample_once(uint16_t *buf)
{
	struct adc_sequence seq = {
		.channels    = BIT(ADC_CH_POOL_TEMP) | BIT(ADC_CH_CAVE_TEMP) |
		               BIT(ADC_CH_LL1) | BIT(ADC_CH_LL2) |
		               BIT(ADC_CH_VTEMP) | BIT(ADC_CH_VBAT),
		.buffer      = buf,
		.buffer_size = ADC_CHANNEL_COUNT * sizeof(uint16_t),
		.resolution  = 12,
	};

	return adc_read(adc_dev, &seq);
}

/*
 * 累加一次 6 通道扫描
 *
 * buf 内容按通道号升序排列 (Zephyr ADC 驱动行为):
 *   buf[0]=CH0(PA0), buf[1]=CH1(PA1), buf[2]=CH7(PA7),
 *   buf[3]=CH9(PB1), buf[4]=CH16(内部温度), buf[5]=CH18(VBAT)
 */
int bsp_adc_internal_sample(void)
{
	uint16_t buf[ADC_CHANNEL_COUNT];

	if (adc_sample_once(buf) < 0)
		return -1;

	k_mutex_lock(&adc_mutex, K_FOREVER);

	accumulator.pool_temp += buf[0];  /* CH0  = PA0 消解池温度 */
	accumulator.cave_temp += buf[1];  /* CH1  = PA1 比色皿温度 */
	accumulator.ll1       += buf[2];  /* CH7  = PA7 低液位 */
	accumulator.ll2       += buf[3];  /* CH9  = PB1 高液位 */
	accumulator.vtemp     += buf[4];  /* CH16 = 内部温度传感器 */
	accumulator.vbat      += buf[5];  /* CH18 = VBAT 电池电压 */

	sample_count++;

	/* 累加满 32 次后计算平均值 (四舍五入) */
	if (sample_count >= ADC_SAMPLE_COUNT) {
		latest_average.pool_temp = (accumulator.pool_temp + ADC_SAMPLE_COUNT/2) / ADC_SAMPLE_COUNT;
		latest_average.cave_temp = (accumulator.cave_temp + ADC_SAMPLE_COUNT/2) / ADC_SAMPLE_COUNT;
		latest_average.ll1       = (accumulator.ll1       + ADC_SAMPLE_COUNT/2) / ADC_SAMPLE_COUNT;
		latest_average.ll2       = (accumulator.ll2       + ADC_SAMPLE_COUNT/2) / ADC_SAMPLE_COUNT;
		latest_average.vtemp     = (accumulator.vtemp     + ADC_SAMPLE_COUNT/2) / ADC_SAMPLE_COUNT;
		latest_average.vbat      = (accumulator.vbat      + ADC_SAMPLE_COUNT/2) / ADC_SAMPLE_COUNT;

		memset(&accumulator, 0, sizeof(accumulator));
		sample_count = 0;
	}

	k_mutex_unlock(&adc_mutex);
	return 0;
}

/* ==================== 获取最新平均值 ==================== */

void bsp_adc_internal_read_average(adc_average_t *avg)
{
	k_mutex_lock(&adc_mutex, K_FOREVER);
	memcpy(avg, &latest_average, sizeof(*avg));
	k_mutex_unlock(&adc_mutex);
}

/*
 * 物理量转换
 *
 * CPU 温度: STM32F4 内部温度传感器
 *   V_sense = code * 3.3 / 4095
 *   Temp = (V_sense - V25) / Avg_Slope + 25
 *   其中 V25=0.76V, Avg_Slope=2.5mV/°C (数据手册典型值)
 *
 * VBAT: STM32F4 内部 1/2 分压
 *   V_bat = code * 2 * 3.3 / 4095
 *
 * 外部通道 (PA0/PA1/PA7/PB1): 仅保存原始 ADC 值
 *   物理量转换 (NTC 温度、液位阈值) 由上位机或 AutoRun 完成
 */
void bsp_adc_internal_convert(const adc_average_t *avg, adc_physical_t *phys)
{
	float temp;

	/* CPU 温度 (用于 PID 温控代理, 消解池温度传感器连接 PA0) */
	temp  = (float)avg->vtemp;
	temp *= 3.3f;
	temp /= 4095.0f;
	temp -= 0.76f;
	temp /= 0.0025f;
	temp += 25.0f;
	phys->cpu_temp = temp;

	/* VBAT 电池电压 (1/2 分压, 实际电压 = 读数 * 2) */
	temp  = (float)avg->vbat * 2.0f;
	temp *= 3.3f;
	temp /= 4095.0f;
	phys->vbat_volt = temp;

	/* 外部通道: 保存原始 ADC 值 (12-bit, 0-4095) */
	phys->pool_temp = (float)avg->pool_temp;
	phys->cave_temp = (float)avg->cave_temp;
}
