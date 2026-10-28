/*
 * autorun.c — AutoRun 20 模块分析流程
 *
 * ==================== 总磷分析原理 ====================
 *
 * 总磷(TP)水质分析采用钼酸铵分光光度法:
 *   1. 水样经高温消解(120°C), 将各种形态的磷全部转化为正磷酸盐
 *   2. 正磷酸盐与钼酸铵试剂反应生成磷钼杂多酸
 *   3. 在还原剂作用下, 磷钼杂多酸被还原成蓝色化合物(钼蓝)
 *   4. 在特定波长处测量吸光度, 根据朗伯-比尔定律计算浓度
 *
 * 吸光度公式: Abs = log10(V1/V2) - RW125 * log10(V1/V3)
 *   V1 = 空白/蒸馏水基准读数
 *   V2 = 反应后读数 (含显色产物)
 *   V3 = 浊度补偿读数 (消除悬浮物干扰)
 *   RW125 = 浊度补偿系数 (上位机可配置)
 *
 * 浓度公式: Concentration = k * Abs + b
 *   k = 校准曲线斜率 (从 pMb3InputData 读取)
 *   b = 校准曲线截距 (从 pMb3InputData 读取)
 *
 * ==================== 流程总览 ====================
 *
 *   模块1 排空管路 → 模块2 润洗 → 模块3 液位校准 → 模块4 外接泵
 *   → 模块5 进样(采水样) → 模块6 进试剂1(空气驱动) → 模块7 加热消解120°C
 *   → 模块8 进蒸馏水 → 模块9 进试剂2 → 模块10 浊度读数V3
 *   → 模块11 进试剂3 → 模块12 静置反应50°C → 模块13 读V2(反应后)
 *   → 模块14 排液清洗 → 模块15 读V1空白 → 模块16 读V1反应
 *   → 模块17 吸光度计算 → 模块18 安全关闭 → 模块19 浓度计算
 *   → 模块20 模式判断(连续/周期/标定/核查)
 *
 * ==================== 阀门位掩码说明 ====================
 *
 * bsp_valve_open_mask/close_mask 使用 32-bit 掩码操作多个阀门:
 *   bit N = valve (N+1), 与 bsp_valve.c 的 1-indexed 阀门编号对应
 *   例如: BIT(12) = 0x1000 = Y13 废液阀
 *
 * 物理功能:
 *   Y1  进水阀    Y2  水样阀    Y3  空气阀/试剂1
 *   Y4  比色阀    Y5  蒸馏水阀  Y6  试剂2阀
 *   Y7  试剂3阀   Y8  量程液阀  Y9  消解下阀
 *   Y10 消解上阀   Y11 冷却风扇  Y13 废液阀
 *   Y14 核查液阀
 *
 * ==================== Modbus 寄存器关联 ====================
 *
 *   RW50  (PDU 107): 工作模式 (1=水样, 2=核查, 3=零标, 20=量程...)
 *   RW4229 (PDU 116): 当前模块号 (上报给上位机, 0=未运行)
 *   RW4230 (PDU 117): 报警码 (0=正常, 100=泵超时, 101=温度超时)
 *   RW80  (PDU 80):  V3 浊度读数 (模块10 写入)
 *   RW81  (PDU 81):  V2 反应后读数 (模块13 写入)
 *   RW82  (PDU 82):  V1 基准读数 (模块15/16 写入)
 */

#include "autorun.h"
#include "task_modbus.h"
#include "task_process_control.h"
#include "bsp/bsp_valve.h"
#include "bsp/bsp_pump.h"
#include "bsp/bsp_pwm.h"
#include "bsp/bsp_pid.h"
#include "bsp/bsp_adc_ads1220.h"
#include "bsp/bsp_adc_internal.h"
#include "task_error_manage.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <math.h>

/* ==================== 超时参数 (生产环境) ==================== */
#define PUMP_TIMEOUT_MS    20000   /* 泵等待超时 20s */
#define PUMP_HD_TIMEOUT_MS 30000   /* 泵推动超时 30s (推入比色皿) */
#define TEMP_TIMEOUT_MS    800000  /* 升温等待超时 800s (~13分钟) */
#define TEMP_COOL_TIMEOUT_MS 500000 /* 冷却等待超时 500s (~8分钟) */

#define TEMP_CTRL_TICK_MS  200     /* PID 控温周期 200ms */

/* RW4230 报警码 (写入保持寄存器, 上位机可读取) */
#define RW4230_PUMP_TIMEOUT  100   /* 泵超时 */
#define RW4230_TEMP_TIMEOUT  101   /* 温度控制超时 */

/*
 * 阀门位掩码常量 (用于 bsp_valve_open_mask/close_mask)
 * bit N = valve (N+1), 与 bsp_valve.c 的 1-indexed 阀门编号对应
 *
 * 物理功能:
 *   Y1  进水阀    Y2  水样阀    Y3  空气阀/试剂1
 *   Y4  比色阀    Y5  蒸馏水阀  Y6  试剂2阀
 *   Y7  试剂3阀   Y8  量程液阀  Y9  消解下阀
 *   Y10 消解上阀   Y11 冷却风扇  Y13 废液阀
 *   Y14 核查液阀
 */
#define V_WATER_IN     0  /* bit0: Y1 进水阀 */
#define V_REAGENT1     1  /* bit1: Y2 水样阀 (也用作试剂1源) */
#define V_AIR          2  /* bit2: Y3 空气阀 (进试剂1时通空气) */
#define V_CUVETTE      3  /* bit3: Y4 比色阀 (连比色皿) */
#define V_SAMPLE       4  /* bit4: Y5 蒸馏水阀 */
#define V_REAGENT2     5  /* bit5: Y6 试剂2阀 */
#define V_REAGENT3     6  /* bit6: Y7 试剂3阀 */
#define V_STD          7  /* bit7: Y8 量程液阀 */
#define V_DIGEST_LOW   8  /* bit8: Y9 消解下阀 */
#define V_DIGEST_HIGH  9  /* bit9: Y10 消解上阀 */
#define V_DRAIN       10  /* bit10: Y11 排液阀 */
#define V_FAN         10  /* bit10: Y11 冷却风扇 (同排液阀) */
#define V_WASTE       12  /* bit12: Y13 废液阀 */
#define V_CHECK       13  /* bit13: Y14 核查液阀 */

/* BIT() 宏由 Zephyr <zephyr/sys/util_macro.h> 提供 */

/* PID 温度控制实例 */
static PID_t heat_pid;

/* ==================== 辅助函数 ==================== */

/**
 * @brief 启动泵 (设置方向 + 启动脉冲 + 使能)
 * @param speed 正值=顺时针(抽), 负值=逆时针(推), 绝对值=频率
 */
static void pump_run(int speed, int circles)
{
	ARG_UNUSED(circles);
	bsp_pump_set_direction(speed > 0 ? MOTOR_CW : MOTOR_CCW);
	bsp_pwm_start_motor((uint16_t)(speed > 0 ? speed : -speed));
	bsp_pump_enable();
	TimeoutFlag = false;
}

/** @brief 停止泵 (停止脉冲 + 禁用) */
static void pump_stop(void)
{
	bsp_pwm_stop_motor();
	bsp_pump_disable();
}

/**
 * @brief 等待标志位 (带超时)
 * @param pFlag     要等待的标志位指针 (泵完成时由 ISR 设置 true)
 * @param timeout_ms 超时时间
 * @param tick_ms    轮询间隔
 * @param pErrorCode 超时时写入的报警码 (RW4230_PUMP_TIMEOUT=100)
 * @return true=标志位置位, false=超时
 */
static bool autorun_wait_flag(volatile bool *pFlag, uint32_t timeout_ms,
			uint32_t tick_ms, uint16_t *pErrorCode)
{
	uint32_t elapsed = 0;
	while (!*pFlag && elapsed < timeout_ms) {
		k_sleep(K_MSEC(tick_ms));
		elapsed += tick_ms;
	}
	if (!*pFlag) {
		if (pErrorCode) *pErrorCode = RW4230_PUMP_TIMEOUT;
		return false;
	}
	return true;
}

/**
 * @brief 等待温度达到目标 (带超时 + PID 控温)
 * @param target      目标温度 (°C)
 * @param wait_lower  true=等待降到目标以下, false=等待升到目标以上
 * @param timeout_ms  超时时间
 * @param tick_ms     PID 控温周期
 * @return true=温度达标, false=超时 (写 RW4230=101)
 *
 * 工作流程:
 *   1. 读取内部 ADC 温度 (CPU 温度作为消解池温度代理)
 *   2. PID 计算输出 → bsp_pwm_set_heat(60000 - output)
 *   3. active-low: output 越大, 加热越强; 60000=不加热
 */
static bool autorun_wait_temp(float target, bool wait_lower,
			      uint32_t timeout_ms, uint32_t tick_ms)
{
	uint32_t elapsed = 0;
	adc_average_t avg;
	adc_physical_t phy;

	while (elapsed < timeout_ms) {
		bsp_adc_internal_sample();
		k_sleep(K_MSEC(10));
		bsp_adc_internal_read_average(&avg);
		bsp_adc_internal_convert(&avg, &phy);
		float temp = phy.cpu_temp;

		if (wait_lower && temp <= target) return true;
		if (!wait_lower && temp >= target) return true;

		/* PID 控温: output 越大加热越强, active-low 取反 */
		int output = PIDRealize(&heat_pid, temp);
		if (output < 0) output = 0;
		if (output > 60000) output = 60000;
		bsp_pwm_set_heat((uint16_t)(60000 - output));

		k_sleep(K_MSEC(tick_ms));
		elapsed += tick_ms;
	}

	pMbHoldData->RW4230 = RW4230_TEMP_TIMEOUT;
	return false;
}

/**
 * @brief ADS1220 多次采样取平均 (用于光谱读数 V1/V2/V3)
 * @param samples 采样次数
 * @param delay_ms 每次采样间隔
 * @return CH3 通道的平均 ADC 值 (右移 8 位后的 16-bit 值)
 */
static uint16_t read_adc_average(int samples, int delay_ms)
{
	uint32_t sum = 0;
	uint16_t buf[4];

	for (int i = 0; i < samples; i++) {
		bsp_ads1220_read_channels(buf, 4);
		sum += buf[0];  /* buf[0] = CH3 光谱通道 */
		k_sleep(K_MSEC(delay_ms));
	}
	return (uint16_t)(sum / samples);
}

/**
 * @brief 通用进样流程
 * @param src_valve_bit 源阀门位编号 (如 V_REAGENT1=1 表示 Y2)
 * @param pump_speed    泵速 (正值=抽液)
 *
 * 流程:
 *   1. 开源阀 → 2. 泵抽液 → 3. 等泵完成/超时 → 4. 关源阀
 *   5. 开 Y4+Y9+Y10 (比色阀+消解阀) → 6. 泵反推入比色皿 → 7. 关阀
 */
static void pump_in_reagent(uint16_t src_valve_bit, int pump_speed)
{
	uint32_t mask;

	/* 开源阀 */
	mask = BIT(src_valve_bit);
	bsp_valve_open_mask(mask);
	k_sleep(K_MSEC(200));

	/* 泵抽 */
	pump_run(pump_speed, 250);
	autorun_wait_flag(&TimeoutFlag, PUMP_TIMEOUT_MS, 100, &pMbHoldData->RW4230);
	pump_stop();
	TimeoutFlag = false;
	k_sleep(K_MSEC(200));

	bsp_valve_close_mask(mask);

	/* 推入比色皿: 开 Y4+Y9+Y10 */
	mask = BIT(V_CUVETTE) | BIT(V_DIGEST_LOW) | BIT(V_DIGEST_HIGH);
	bsp_valve_open_mask(mask);
	k_sleep(K_MSEC(200));

	pump_run(-1200, 250);
	autorun_wait_flag(&TimeoutFlag, PUMP_HD_TIMEOUT_MS, 100, &pMbHoldData->RW4230);
	pump_stop();
	TimeoutFlag = false;

	bsp_valve_close_mask(mask);
	k_sleep(K_MSEC(200));
}

/**
 * @brief 启动 PID 温度控制
 * @param target 目标温度 (°C)
 * @param kp/ki/kd PID 参数 (模块7: 3600/8/800, 模块12: 3800/6/800)
 */
static void start_temp_control(float target, float kp, float ki, float kd)
{
	ClearPidParam(&heat_pid);
	SetPidParam(&heat_pid, kp, ki, kd, 0.5f);
	SetPidMaxUk(&heat_pid, 60000);
	SetTargetValue(&heat_pid, target);
}

/**
 * @brief 停止加热 + 开风扇冷却到目标温度
 * @param cool_target 冷却目标温度 (°C)
 *
 * 流程: 停止加热 → 开 Y11 风扇 → 等温度降到目标 → 关风扇 → 确认停止加热
 */
static void stop_and_cool(float cool_target)
{
	bsp_pwm_set_heat(60000);  /* 停止加热 (active-low: 60000=0% 占空比) */

	/* 开风扇冷却 */
	bsp_valve_open(V_FAN + 1);  /* valve_name_t 是 1-indexed, V_FAN=10 → valve 11 */
	autorun_wait_temp(cool_target, true, TEMP_COOL_TIMEOUT_MS, TEMP_CTRL_TICK_MS);
	bsp_valve_close(V_FAN + 1);
	bsp_pwm_set_heat(60000);  /* 确认停止加热 */
}

/*
 * ==================== 模块实现 ====================
 *
 * 每个模块函数:
 *   - 返回下一模块编号 (如 MOD_ZERO_RINSE)
 *   - 返回 0 退出 AutoRun
 *   - 超时/错误时跳转 MOD_FINISH (模块 18 安全关闭)
 *   - 打印 printk 日志 (模块号 + 状态)
 */

/*
 * 模块 1: 排空管路 + 排空比色皿
 *
 * 功能: 测量开始前的清洁准备, 确保管路和比色皿中无残留液体
 *
 * 物理过程:
 *   step1: 关闭所有阀门 → 打开 Y13 废液阀 → 泵反转(负速度)将管路中残留液体排入废液桶
 *   step2: 泵正转抽气(闷抽) → 打开 Y4+Y9+Y10(比色阀+消解上下阀) → 泵继续正转将比色皿中液体抽出
 *
 * 为什么需要两步:
 *   - step1 排空的是管路(阀门到泵之间的管段)
 *   - step2 排空的是比色皿(消解池+比色池), 需要先打开比色通路的阀门
 *
 * 返回: MOD_ZERO_RINSE (模块2) — 进入润洗流程
 */
static uint8_t mod_drain(void)
{
	uint32_t mask;

	printk("AutoRun[1]: Drain\n");
	pMbHoldData->RW4230 = 0;         /* 清除上次报警码 */
	pMbInputData->nStepCode = 1;     /* 上报步骤码给上位机 */

	/* step1: 排空管路 — 关闭所有阀门, 然后只打开废液阀 Y13 */
	bsp_valve_close_all();
	k_sleep(K_MSEC(200));            /* 等待阀门动作完成 */

	mask = BIT(V_WASTE);             /* 只打开 Y13 废液阀 */
	bsp_valve_open_mask(mask);
	k_sleep(K_MSEC(200));

	/* 泵反转 -1200Hz: 将管路中残留液体排向废液方向 */
	pump_run(-1200, 250);
	if (!autorun_wait_flag(&TimeoutFlag, PUMP_TIMEOUT_MS, 100, &pMbHoldData->RW4230))
		pump_stop();                 /* 超时则强制停止泵 */
	TimeoutFlag = false;
	k_sleep(K_MSEC(200));
	bsp_valve_close_mask(mask);      /* 关闭废液阀 */

	/* step2: 闷抽比色皿 — 先正转抽气(建立负压), 再打开比色通路 */
	pump_run(1000, 150);
	if (!autorun_wait_flag(&TimeoutFlag, PUMP_TIMEOUT_MS, 100, &pMbHoldData->RW4230))
		pump_stop();
	TimeoutFlag = false;

	/* 打开比色通路: Y4(比色阀) + Y9(消解下阀) + Y10(消解上阀) */
	mask = BIT(V_CUVETTE) | BIT(V_DIGEST_LOW) | BIT(V_DIGEST_HIGH);
	bsp_valve_open_mask(mask);
	k_sleep(K_MSEC(200));

	/* 正转 1000Hz: 将比色皿中液体抽出, 超时 30s (推入比色皿需要更长时间) */
	pump_run(1000, 150);
	if (!autorun_wait_flag(&TimeoutFlag, PUMP_HD_TIMEOUT_MS, 100, &pMbHoldData->RW4230))
		pump_stop();
	TimeoutFlag = false;

	bsp_valve_close_mask(mask);      /* 关闭比色通路 */
	k_sleep(K_MSEC(200));

	return MOD_ZERO_RINSE;           /* 进入模块2: 润洗 */
}

/*
 * 模块 2: 零标流程 / 润洗
 *
 * 功能: 用蒸馏水清洗管路, 消除上次测量残留物对本次测量的干扰
 *
 * 工作模式判断 (RW50):
 *   RW50=1(连续水样), 6(定点水样), 7(手动水样): 需要润洗 → 进入模块3
 *   其他模式(核查/标定/量程): 不需要润洗 → 跳转模块4(外接泵)
 *
 * 润洗原理:
 *   蒸馏水通过管路循环, 冲洗掉残留的试剂和样品,
 *   避免交叉污染影响测量精度
 *
 * TODO: 当前直接跳转, 后续补充润洗流程(抽蒸馏水→循环→排空)
 *
 * 返回: MOD_ZERO_LEVEL_CAL(模块3) 或 MOD_EXT_PUMP(模块4)
 */
static uint8_t mod_zero_rinse(void)
{
	printk("AutoRun[2]: Zero rinse\n");
	/* 非水样模式直接跳转模块4, 不需要润洗 */
	if (pMbHoldData->RW50 != 1 && pMbHoldData->RW50 != 6 && pMbHoldData->RW50 != 7) {
		return MOD_EXT_PUMP;
	}
	/* TODO: 润洗流程 (抽蒸馏水→循环清洗→排空) */
	return MOD_ZERO_LEVEL_CAL;
}

/*
 * 模块 3: 零点标定液位校准
 *
 * 功能: 校准液位传感器的零点, 确保液位检测准确
 *
 * 校准原理:
 *   液位传感器通过 ADC 读取模拟电压判断液位高低
 *   零点校准: 在无液体状态下读取传感器基准值
 *   高低液位校验: 确认高液位和低液位信号能正确触发
 *
 * 重试机制: 最多重试 5 次, 超过后跳过校准继续流程
 *   (实际生产中, 液位校准失败不应阻塞整个测量流程)
 *
 * TODO: 当前直接跳过, 后续补充 AD 校准 + 高低液位信号校验逻辑
 *
 * 返回: MOD_EXT_PUMP (模块4) — 进入外接泵循环
 */
static uint8_t mod_zero_level_cal(void)
{
	static int retry;
	printk("AutoRun[3]: Zero level cal (retry=%d)\n", retry);
	retry++;
	if (retry >= 5) { retry = 0; return MOD_EXT_PUMP; }
	/* TODO: AD 校准(读取无液体时的ADC基准值) + 高低液位校验(确认信号能正确触发) */
	return MOD_EXT_PUMP;  /* 暂时跳过 */
}

/*
 * 模块 4: 外接泵循环
 *
 * 功能: 控制外接采样水泵, 从水源(河流/湖泊/排污口)抽取水样
 *
 * 工作原理:
 *   外接水泵通过继电器控制(预留 GPIO), 将水样从水源输送到仪器进样口
 *   水泵运行时间由上位机配置, 确保管路充满新鲜水样
 *
 * 跳转逻辑:
 *   使用采样器: 等待采样器信号 → 模块30(采样器等待) → 模块5(进样)
 *   不使用采样器: 直接进入模块5(进样)
 *
 * TODO: 当前直接跳转模块5, 后续补充外接泵控制逻辑
 *
 * 返回: MOD_SAMPLING (模块5) — 进入进样流程
 */
static uint8_t mod_ext_pump(void)
{
	printk("AutoRun[4]: External pump\n");
	/* TODO: 控制外接水泵开/关, 根据是否使用采样器决定跳转目标 */
	return MOD_SAMPLING;
}

/*
 * 模块 5: 进样 (采水样) — 根据 RW50 选择源阀
 *
 * 功能: 从对应容器中抽取样品/标液, 并推入比色皿(消解池)
 *
 * RW50 工作模式与源阀对应关系:
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │ RW50 值        │ 模式名称       │ 源阀        │ 物理容器  │
 *   ├─────────────────────────────────────────────────────────────┤
 *   │ 1,5,6,7,43     │ 水样测量       │ Y2 (bit1)   │ 水样桶    │
 *   │ 2              │ 标液核查       │ Y14 (bit13) │ 核查液桶  │
 *   │ 3,19,21,24,31  │ 零点标定       │ Y5 (bit4)   │ 蒸馏水桶  │
 *   │ 20,22,25,32    │ 量程标定       │ Y8 (bit7)   │ 量程液桶  │
 *   │ 默认           │ 其他模式       │ Y2 (bit1)   │ 水样桶    │
 *   └─────────────────────────────────────────────────────────────┘
 *
 * 进样流程 (pump_in_reagent):
 *   1. 打开源阀(如 Y2) → 等待 200ms 阀门动作
 *   2. 泵正转 600Hz 抽液 → 等待泵完成或超时(20s)
 *   3. 关闭源阀
 *   4. 打开比色通路(Y4+Y9+Y10) → 泵反转 1200Hz 推入比色皿(超时30s)
 *   5. 关闭比色通路
 *
 * 为什么选择 600Hz: 蠕动泵低速抽液, 避免产生气泡影响测量精度
 * 为什么推入比色皿用 1200Hz: 反向推液需要更大压力, 速度更高
 *
 * 返回: MOD_REAGENT1 (模块6) — 进入进试剂1流程
 */
static uint8_t mod_sampling(void)
{
	uint16_t src_bit;
	uint16_t rw50 = pMbHoldData->RW50;

	printk("AutoRun[5]: Sampling (RW50=%u)\n", rw50);

	/* 根据工作模式选择源阀门 */
	switch (rw50) {
	case 1: case 5: case 6: case 7: case 43:
		src_bit = V_REAGENT1;  /* Y2 水样阀: 从水样桶抽取待测水样 */
		break;
	case 2:
		src_bit = V_CHECK;     /* Y14 核查液阀: 从核查液桶抽取标液 */
		break;
	case 3: case 19: case 21: case 24: case 31:
		src_bit = V_SAMPLE;    /* Y5 蒸馏水阀: 从蒸馏水桶抽取(零点标定用) */
		break;
	case 20: case 22: case 25: case 32:
		src_bit = V_STD;       /* Y8 量程液阀: 从量程液桶抽取(量程标定用) */
		break;
	default:
		src_bit = V_REAGENT1;  /* 默认: 从水样桶抽取 */
		break;
	}

	pump_in_reagent(src_bit, 600);  /* 600Hz 低速抽液, 避免气泡 */
	return MOD_REAGENT1;
}

/*
 * 模块 6: 进试剂 1 (空气驱动)
 *
 * 功能: 通过 Y3(空气阀) 将空气注入管路, 作为试剂1的驱动介质
 *
 * 为什么用空气而不是直接抽试剂:
 *   - 蠕动泵直接抽试剂会残留试剂在管路中, 造成交叉污染
 *   - 空气作为"活塞", 将管路中已有的试剂1推入比色皿
 *   - 空气驱动可以精确控制试剂体积(通过泵的圈数/时间)
 *
 * 物理过程:
 *   打开 Y3(空气阀) → 泵正转 600Hz 抽空气 → 关闭 Y3
 *   → 打开 Y4+Y9+Y10(比色通路) → 泵反转 1200Hz 将空气推入比色皿
 *   → 空气将管路中的试剂1推入消解池 → 关闭比色通路
 *
 * 返回: MOD_HEAT_DIGEST (模块7) — 进入加热消解流程
 */
static uint8_t mod_reagent1(void)
{
	printk("AutoRun[6]: Reagent 1\n");
	pump_in_reagent(V_AIR, 600);  /* Y3 空气阀: 抽空气驱动试剂1 */
	return MOD_HEAT_DIGEST;
}

/*
 * 模块 7: 加热消解 120°C, 保持 5 分钟
 *
 * 功能: 高温消解水样, 将各种形态的磷(有机磷、聚磷酸盐等)全部转化为正磷酸盐
 *
 * 消解原理:
 *   在高温(120°C)和酸性条件下, 强氧化剂(过硫酸钾等)将水样中的磷化合物氧化分解
 *   消解时间 5 分钟(300秒), 确保完全转化
 *   消解温度必须精确控制: 温度过低→消解不完全→结果偏低
 *                              温度过高→试剂分解→结果偏高
 *
 * PID 控温参数:
 *   Kp=3600 (比例系数): 较大, 快速响应温度偏差
 *   Ki=8    (积分系数): 消除稳态误差
 *   Kd=800  (微分系数): 抑制超调, 避免温度过冲
 *   目标温度: 120°C, 到达阈值: 119.1°C (留 0.9°C 余量避免频繁切换)
 *
 * 升温过程:
 *   1. 启动 PID 控温 → 加热器 PWM 输出
 *   2. 每 200ms 读取温度(ADC 内部温度传感器作为代理) → PID 计算 → 更新 PWM
 *   3. 温度达到 119.1°C 后, 保持加热 5 分钟(维持 120°C)
 *   4. 停止加热 → 开 Y11 冷却风扇 → 等温度降到 50°C 以下
 *
 * 冷却为什么降到 50°C:
 *   - 50°C 是后续模块(进蒸馏水/试剂)的最佳工作温度
 *   - 温度过高会加速试剂挥发, 影响测量精度
 *
 * 超时处理:
 *   升温超时(800s≈13分钟): 报警 HOT_TIMEOUT_ERROR, 跳转模块18(安全关闭)
 *   冷却超时(500s≈8分钟):  等待完成(不中断流程, 风扇持续工作)
 *
 * 返回: MOD_DISTILLED_WATER (模块8) — 进入进蒸馏水流程
 */
static uint8_t mod_heat_digest(void)
{
	printk("AutoRun[7]: Heat digest 120C\n");

	/* 启动 PID 控温: 目标 120°C */
	start_temp_control(120.0f, 3600.0f, 8.0f, 800.0f);
	pMbHoldData->nTestTemper = 120;      /* 上报给上位机: 当前消解温度 */
	pMbHoldData->nTestTemperHoldtime = 5; /* 上报给上位机: 保持时间(分钟) */

	/* 等待升温到 119.1°C (超时 800s≈13分钟) */
	if (!autorun_wait_temp(119.1f, false, TEMP_TIMEOUT_MS, TEMP_CTRL_TICK_MS)) {
		SendFunErrorCode(HOT_TIMEOUT_ERROR);  /* 超时报警 */
		return MOD_FINISH;                      /* 跳转安全关闭 */
	}

	/* 保持 5 分钟(300秒): PID 持续控温, 维持 120°C */
	printk("AutoRun[7]: Reached 120C, holding 5min...\n");
	k_sleep(K_MSEC(300000));  /* 300秒 = 5分钟 */

	/* 冷却到 50°C: 停止加热 → 开风扇 → 等温度降到 49.3°C 以下 */
	printk("AutoRun[7]: Cooling to 50C\n");
	stop_and_cool(49.3f);

	printk("AutoRun[7]: Heat digest done\n");
	return MOD_DISTILLED_WATER;
}

/*
 * 模块 8: 进蒸馏水
 *
 * 功能: 向消解池中加入蒸馏水, 稀释消解后的样品溶液
 *
 * 为什么要加蒸馏水:
 *   - 消解后溶液浓度较高, 需要稀释到比色法的线性范围内
 *   - 蒸馏水作为溶剂, 为后续试剂反应提供液体环境
 *   - 蒸馏水本身不含磷, 不会引入干扰
 *
 * 物理过程: 打开 Y5(蒸馏水阀) → 泵正转 600Hz 抽蒸馏水
 *   → 关闭 Y5 → 打开比色通路 → 泵反转推入比色皿 → 关闭比色通路
 *
 * 返回: MOD_REAGENT2 (模块9) — 进入进试剂2流程
 */
static uint8_t mod_distilled_water(void)
{
	printk("AutoRun[8]: Distilled water\n");
	pump_in_reagent(V_SAMPLE, 600);  /* Y5 蒸馏水阀: 抽蒸馏水稀释样品 */
	return MOD_REAGENT2;
}

/*
 * 模块 9: 进试剂 2 (钼酸铵溶液)
 *
 * 功能: 向比色皿中加入钼酸铵试剂, 与正磷酸盐反应生成磷钼杂多酸
 *
 * 试剂2成分: 钼酸铵溶液 (通常含酒石酸锑钾作为催化剂)
 * 反应原理: PO4³⁻ + 12MoO4²⁻ + 24H⁺ → H3[P(Mo3O10)4] + 12H2O
 *   正磷酸盐与钼酸铵在酸性条件下生成黄色的磷钼杂多酸
 *
 * 物理过程: 打开 Y6(试剂2阀) → 泵正转 600Hz 抽试剂
 *   → 关闭 Y6 → 打开比色通路 → 泵反转推入比色皿 → 关闭比色通路
 *
 * 返回: MOD_TURBIDITY (模块10) — 进入浊度补偿读数
 */
static uint8_t mod_reagent2(void)
{
	printk("AutoRun[9]: Reagent 2\n");
	pump_in_reagent(V_REAGENT2, 600);  /* Y6 试剂2阀: 抽钼酸铵试剂 */
	return MOD_TURBIDITY;
}

/*
 * 模块 10: 浊度补偿 V3 读数
 *
 * 功能: 在加入显色剂(试剂3)之前, 测量溶液的浊度值 V3
 *
 * 为什么要测浊度:
 *   - 水样中的悬浮物(泥沙、有机颗粒)会散射光线, 干扰吸光度测量
 *   - 浊度补偿公式: Abs = log10(V1/V2) - RW125 * log10(V1/V3)
 *   - V3 是未显色时的透光率, 反映悬浮物的散射程度
 *   - RW125 是浊度补偿系数(上位机可配置, 为0则不补偿)
 *
 * ADS1220 读数:
 *   - 读取 CH3 通道(光谱传感器)的 ADC 值
 *   - 10 次采样取平均, 减少随机噪声
 *   - 每次采样间隔 200ms, 确保传感器稳定
 *   - 存入 RW80, 供模块17(吸光度计算)使用
 *
 * 返回: MOD_REAGENT3 (模块11) — 进入进试剂3流程
 */
static uint8_t mod_turbidity(void)
{
	printk("AutoRun[10]: Turbidity V3\n");
	uint16_t v3 = read_adc_average(10, 200);  /* 10次采样取平均 */
	pMbHoldData->RW80 = v3;                   /* 存入 RW80, 供吸光度计算 */
	printk("AutoRun[10]: V3=%u\n", v3);
	return MOD_REAGENT3;
}

/*
 * 模块 11: 进试剂 3 (还原剂)
 *
 * 功能: 向比色皿中加入还原剂, 将磷钼杂多酸还原成蓝色化合物(钼蓝)
 *
 * 试剂3成分: 抗坏血酸(维生素C)或氯化亚锡等还原剂
 * 反应原理: H3[P(Mo3O10)4] + 还原剂 → 钼蓝(蓝色化合物)
 *   黄色的磷钼杂多酸被还原成深蓝色的钼蓝, 颜色深浅与磷浓度成正比
 *   这就是比色法测量的基础: 颜色越深, 吸光度越大, 磷浓度越高
 *
 * 物理过程: 打开 Y7(试剂3阀) → 泵正转 600Hz 抽还原剂
 *   → 关闭 Y7 → 打开比色通路 → 泵反转推入比色皿 → 关闭比色通路
 *
 * 返回: MOD_SETTLE_REACT (模块12) — 进入静置反应流程
 */
static uint8_t mod_reagent3(void)
{
	printk("AutoRun[11]: Reagent 3\n");
	pump_in_reagent(V_REAGENT3, 600);  /* Y7 试剂3阀: 抽还原剂(生成钼蓝) */
	return MOD_SETTLE_REACT;
}

/*
 * 模块 12: 静置反应 50°C, 保持 5 分钟
 *
 * 功能: 在恒温 50°C 条件下, 让显色反应充分进行
 *
 * 为什么要 50°C:
 *   - 钼蓝显色反应在室温下较慢, 50°C 可加速反应(约 5 分钟完成)
 *   - 温度过高(>60°C)会导致钼蓝分解, 颜色褪色
 *   - 50°C 是反应速率和稳定性的最佳平衡点
 *
 * 为什么要保持 5 分钟:
 *   - 显色反应需要一定时间才能达到平衡
 *   - 时间太短→反应不完全→颜色偏浅→结果偏低
 *   - 时间太长→钼蓝可能缓慢分解→颜色偏浅
 *   - 5 分钟是实验确定的最佳反应时间
 *
 * PID 控温参数:
 *   Kp=3800, Ki=6, Kd=800 — 与模块7略有不同(目标温度更低, 响应特性不同)
 *   目标温度: 50°C, 到达阈值: 49.3°C
 *
 * 返回: MOD_READ_V2 (模块13) — 进入读取 V2(反应后读数)
 */
static uint8_t mod_settle_react(void)
{
	printk("AutoRun[12]: Settle react 50C\n");

	/* 启动 PID 控温: 目标 50°C */
	start_temp_control(50.0f, 3800.0f, 6.0f, 800.0f);
	pMbHoldData->nTestTemper = 50;       /* 上报: 当前反应温度 */
	pMbHoldData->nTestTemperHoldtime = 5; /* 上报: 保持时间(分钟) */

	/* 保持 5 分钟: 让显色反应充分进行 */
	k_sleep(K_MSEC(300000));

	/* 冷却到 50°C 以下: 为后续光谱读数做准备 */
	stop_and_cool(49.3f);

	printk("AutoRun[12]: Settle react done\n");
	return MOD_READ_V2;
}

/*
 * 模块 13: 读取 V2 (反应后光谱读数)
 *
 * 功能: 测量显色反应后溶液的透光率 V2
 *
 * V2 的物理意义:
 *   - V2 是光源穿过显色溶液后, 光谱传感器(CH3)接收到的光强
 *   - 溶液中钼蓝颜色越深(磷浓度越高), V2 值越小(透光率越低)
 *   - V2 与 V1(基准)的比值决定吸光度: Abs = log10(V1/V2)
 *
 * ADS1220 读数:
 *   - 10 次采样取平均, 每次间隔 200ms
 *   - 存入 RW81, 供模块17(吸光度计算)使用
 *
 * 返回: MOD_DRAIN_CLEAN (模块14) — 进入排液清洗流程
 */
static uint8_t mod_read_v2(void)
{
	printk("AutoRun[13]: Read V2\n");
	uint16_t v2 = read_adc_average(10, 200);  /* 10次采样取平均 */
	pMbHoldData->RW81 = v2;                   /* 存入 RW81 */
	printk("AutoRun[13]: V2=%u\n", v2);
	return MOD_DRAIN_CLEAN;
}

/*
 * 模块 14: 排液清洗
 *
 * 功能: 排空比色皿中的反应液, 用蒸馏水清洗, 再排空, 为 V1 测量做准备
 *
 * 为什么要清洗:
 *   - 比色皿内壁可能残留显色产物(钼蓝), 影响 V1 基准读数
 *   - 蒸馏水清洗可以去除残留, 确保 V1 测量的准确性
 *
 * 清洗流程(三步):
 *   1. 排空: 打开 Y13(废液阀) → 泵正转 → 将反应液排入废液桶
 *   2. 清洗: 打开 Y5(蒸馏水阀) → 泵正转 → 蒸馏水流入比色皿
 *   3. 排空: 打开 Y13(废液阀) → 泵正转 → 将清洗水排入废液桶
 *
 * 泵速选择:
 *   - 排空用 1000Hz: 较快速度, 尽快排空
 *   - 清洗用 800Hz:  中等速度, 确保充分冲洗
 *
 * 返回: MOD_READ_V1_BLANK (模块15) — 进入读取 V1 空白值
 */
static uint8_t mod_drain_clean(void)
{
	uint32_t mask;
	printk("AutoRun[14]: Drain + clean\n");

	/* 第一步: 排空比色皿 — 将反应液排入废液桶 */
	mask = BIT(V_WASTE);             /* 打开 Y13 废液阀 */
	bsp_valve_open_mask(mask);
	pump_run(1000, 250);             /* 泵正转 1000Hz 快速排空 */
	autorun_wait_flag(&TimeoutFlag, PUMP_TIMEOUT_MS, 100, &pMbHoldData->RW4230);
	pump_stop();
	TimeoutFlag = false;
	bsp_valve_close_mask(mask);

	/* 第二步: 蒸馏水清洗 — 用蒸馏水冲洗比色皿内壁 */
	mask = BIT(V_SAMPLE);            /* 打开 Y5 蒸馏水阀 */
	bsp_valve_open_mask(mask);
	pump_run(800, 250);              /* 泵正转 800Hz 中速清洗 */
	autorun_wait_flag(&TimeoutFlag, PUMP_TIMEOUT_MS, 100, &pMbHoldData->RW4230);
	pump_stop();
	TimeoutFlag = false;
	bsp_valve_close_mask(mask);

	/* 第三步: 再次排空 — 将清洗水排入废液桶 */
	mask = BIT(V_WASTE);             /* 打开 Y13 废液阀 */
	bsp_valve_open_mask(mask);
	pump_run(1000, 250);             /* 泵正转 1000Hz 排空清洗水 */
	autorun_wait_flag(&TimeoutFlag, PUMP_TIMEOUT_MS, 100, &pMbHoldData->RW4230);
	pump_stop();
	TimeoutFlag = false;
	bsp_valve_close_mask(mask);

	return MOD_READ_V1_BLANK;
}

/*
 * 模块 15: 读取 V1 空白值 (蒸馏水基准)
 *
 * 功能: 向比色皿中注入蒸馏水, 测量空白基准值 V1
 *
 * V1 的物理意义:
 *   - V1 是光源穿过纯净蒸馏水后, 光谱传感器接收到的光强
 *   - 代表 100% 透光率(无吸收), 作为吸光度计算的分母基准
 *   - V1 越大, 说明光源越强/传感器灵敏度越高
 *
 * 为什么用蒸馏水:
 *   - 蒸馏水不含任何溶质, 不吸收光线
 *   - 作为"零吸收"基准, 确保吸光度计算的准确性
 *
 * ADS1220 读数:
 *   - 5 次采样取平均(比 V2 少, 因为蒸馏水读数稳定)
 *   - 存入 RW82, 供模块17(吸光度计算)使用
 *
 * 返回: MOD_READ_V1_REACT (模块16) — 进入读取 V1 反应值
 */
static uint8_t mod_read_v1_blank(void)
{
	printk("AutoRun[15]: Read V1 blank\n");

	/* 向比色皿注入蒸馏水 */
	pump_in_reagent(V_SAMPLE, 600);  /* Y5 蒸馏水阀 */

	/* 读取 V1 基准值: 5次采样取平均 */
	uint16_t v1 = read_adc_average(5, 200);
	pMbHoldData->RW82 = v1;         /* 存入 RW82 */
	printk("AutoRun[15]: V1=%u\n", v1);
	return MOD_READ_V1_REACT;
}

/*
 * 模块 16: 读取 V1 反应值 (最终读数)
 *
 * 功能: 再次读取比色皿中的透光率, 作为 V1 最终值
 *
 * 为什么需要两次 V1 读数(模块15和16):
 *   - 模块15: 读取蒸馏水空白值(纯蒸馏水基准)
 *   - 模块16: 读取最终值(可能含微量残留, 确认清洗效果)
 *   - 取最后一次读数作为 V1, 确保数据最新
 *
 * 注意: 当前实现覆盖了 RW82 的值(模块15写的值被覆盖)
 *       如果需要区分空白值和反应值, 应使用不同的寄存器
 *
 * 返回: MOD_ABSORBANCE (模块17) — 进入吸光度计算
 */
static uint8_t mod_read_v1_react(void)
{
	printk("AutoRun[16]: Read V1 react\n");
	uint16_t v1 = read_adc_average(5, 200);
	pMbHoldData->RW82 = v1;  /* 覆盖模块15的值, 使用最新读数 */
	printk("AutoRun[16]: V1=%u\n", v1);
	return MOD_ABSORBANCE;
}

/*
 * 模块 17: 吸光度计算
 *
 * 功能: 根据 V1(基准)、V2(反应后)、V3(浊度) 计算吸光度
 *
 * ==================== 朗伯-比尔定律 ====================
 *
 *   A = ε × l × c
 *
 *   A  = 吸光度 (Absorbance, 无量纲)
 *   ε  = 摩尔吸光系数 (与物质种类和波长有关)
 *   l  = 光程长度 (比色皿宽度, 固定)
 *   c  = 溶液浓度 (待求量)
 *
 *   对于本仪器: A = log10(I₀/I) = log10(V1/V2)
 *     I₀ = 入射光强 (V1, 蒸馏水基准)
 *     I  = 透射光强 (V2, 显色溶液)
 *
 * ==================== 浊度补偿 ====================
 *
 *   水样中的悬浮物会散射光线, 导致 V2 偏小(看起来吸收更多)
 *   但这不是真正的化学吸收, 需要扣除:
 *
 *   Abs = log10(V1/V2) - RW125 × log10(V1/V3)
 *
 *   V3   = 未显色时的透光率 (模块10读取, 反映悬浮物散射)
 *   RW125 = 浊度补偿系数 (上位机配置, 为0则不补偿)
 *   RW125 × log10(V1/V3) = 需要扣除的浊度吸光度
 *
 *   当 RW125=0 或 V3≤0 时, 不进行浊度补偿
 *
 * 返回: MOD_FINISH (模块18) — 进入安全关闭流程
 */
static uint8_t mod_absorbance(void)
{
	float v1 = (float)pMbHoldData->RW82;  /* V1: 蒸馏水基准读数 */
	float v2 = (float)pMbHoldData->RW81;  /* V2: 反应后读数 */

	printk("AutoRun[17]: Absorbance V1=%u V2=%u\n",
	       pMbHoldData->RW82, pMbHoldData->RW81);

	/* 防止除零: V2=0 说明传感器异常或溶液完全不透光 */
	if (v2 <= 0.0f) {
		printk("AutoRun[17]: V2=0, skip calc\n");
		pMbHoldData->fAbsorbanc = 0;
		return MOD_FINISH;
	}

	/* 基本吸光度: A = log10(V1/V2) */
	float abs = log10f(v1 / v2);

	/* 浊度补偿: 扣除悬浮物散射造成的假吸收 */
	if (pMbHoldData->RW125 != 0.0f) {
		float v3 = (float)pMbHoldData->RW80;  /* V3: 浊度读数 */
		if (v3 > 0.0f) {
			abs -= pMbHoldData->RW125 * log10f(v1 / v3);
		}
	}

	pMbHoldData->fAbsorbanc = abs;
	printk("AutoRun[17]: Abs=%d.%04d\n", (int)abs, (int)(abs * 10000) % 10000);
	return MOD_FINISH;
}

/*
 * 模块 18: 流程结束, 安全关闭所有外设
 *
 * 功能: 测量完成或异常中断时, 确保所有执行机构回到安全状态
 *
 * 安全关闭序列:
 *   1. 关闭所有阀门 — 防止液体泄漏或虹吸
 *   2. 停止泵 — 防止泵空转损坏
 *   3. 停止加热 — 防止过热(加热器是 active-low, 60000=0%占空比=不加热)
 *
 * 注意: 此模块在以下情况被调用:
 *   - 正常流程结束(模块17完成后)
 *   - 异常中断(升温超时等错误, SendFunErrorCode 后跳转)
 *   - 任何模块返回 MOD_FINISH
 *
 * 返回: MOD_CALC_RESULT (模块19) — 进入浓度计算
 */
static uint8_t mod_finish(void)
{
	printk("AutoRun[18]: Finish, close devices\n");
	bsp_valve_close_all();           /* 关闭所有阀门 */
	pump_stop();                     /* 停止泵 */
	bsp_pwm_set_heat(60000);        /* 停止加热 (active-low: 60000=不加热) */
	return MOD_CALC_RESULT;
}

/*
 * ==================== MB3 float 辅助函数 ====================
 *
 * Modbus 寄存器是 16-bit, float 需要拆分为两个 uint16_t 传输:
 *   - mb3_float(lo, hi): 从两个 16-bit 寄存器还原 float
 *   - mb3_set_float(lo, hi, val): 将 float 拆分为两个 16-bit 寄存器
 *
 * 字节序: STM32 是小端(little-endian), lo 在低地址, hi 在高地址
 */

/** @brief 从 MB3 uint16_t 低/高字对还原 float */
static float mb3_float(uint16_t lo, uint16_t hi)
{
	union { uint16_t u[2]; float f; } u;
	u.u[0] = lo;  /* 低 16 位 */
	u.u[1] = hi;  /* 高 16 位 */
	return u.f;
}

/** @brief 将 float 拆分为 MB3 uint16_t 低/高字对 */
static void mb3_set_float(uint16_t *lo, uint16_t *hi, float val)
{
	union { uint16_t u[2]; float f; } u;
	u.f = val;
	*lo = u.u[0];  /* 低 16 位 */
	*hi = u.u[1];  /* 高 16 位 */
}

/*
 * 模块 19: 浓度计算 + 结果上报
 *
 * 功能: 根据吸光度和校准曲线, 计算总磷浓度, 并上报给管控仪
 *
 * ==================== 校准曲线 ====================
 *
 *   浓度 = k × 吸光度 + b
 *
 *   k = 校准曲线斜率 (从 pMb3InputData->kL/kH 读取, float 格式)
 *   b = 校准曲线截距 (从 pMb3InputData->bL/bH 读取, float 格式)
 *   k 和 b 由上位机在标定过程中计算并写入 MB3 寄存器
 *
 * ==================== 结果上报 ====================
 *
 *   1. 写入 pMbHoldData: 浓度值(fConcentrationVale)、k/b 参数、吸光度
 *   2. 写入 pMb3InputData: 浓度值(TestDataL/H)、吸光度(TestAborbanL/H)
 *   3. 设置 TestResultFlag=1: 通知管控仪有新结果可读
 *
 * 返回: MOD_MODE_JUDGE (模块20) — 进入模式判断
 */
static uint8_t mod_calc_result(void)
{
	printk("AutoRun[19]: Calc result + mode\n");

	/* 读取吸光度和校准参数 */
	float abs = pMbHoldData->fAbsorbanc;
	float k = mb3_float(pMb3InputData->kL, pMb3InputData->kH);  /* 斜率 */
	float b = mb3_float(pMb3InputData->bL, pMb3InputData->bH);  /* 截距 */

	/* 计算浓度: C = k × A + b */
	float concentration = k * abs + b;
	pMbHoldData->fConcentrationVale = concentration;  /* 存入保持寄存器 */
	pMbHoldData->fK = k;  /* 保存 k 供上位机读取 */
	pMbHoldData->fB = b;  /* 保存 b 供上位机读取 */

	/* 写入 MB3 管控仪寄存器 (通过 USART3 上报) */
	mb3_set_float(&pMb3InputData->TestDataL, &pMb3InputData->TestDataH, concentration);
	mb3_set_float(&pMb3InputData->TestAborbanL, &pMb3InputData->TestAborbanH, abs);
	pMb3InputData->TestResultFlag = 1;  /* 通知管控仪: 有新结果 */

	printk("AutoRun[19]: k=%d.%02d b=%d.%02d abs=%d.%04d conc=%d.%04d\n",
	       (int)k, (int)(k * 100) % 100,
	       (int)b, (int)(b * 100) % 100,
	       (int)abs, (int)(abs * 10000) % 10000,
	       (int)concentration, (int)(concentration * 10000) % 10000);
	return MOD_MODE_JUDGE;
}

/*
 * 模块 20: 模式判断 (根据 RW50 设置完成标志)
 *
 * 功能: 根据工作模式(RW50)设置对应的完成标志, 通知上位机测量结束
 *
 * ==================== 工作模式与完成标志 ====================
 *
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │ RW50 值        │ 模式名称       │ 完成标志                  │
 *   ├─────────────────────────────────────────────────────────────┤
 *   │ 1,5,6,7,43     │ 水样测量       │ nMeasureComplet=1         │
 *   │                │                │ TestSampleFlag=1 (MB3)    │
 *   │ 3,19,21,24,31  │ 零点标定       │ nZeroComplet=1            │
 *   │ 20,22,25,32    │ 量程标定       │ nMeasureComplet=1         │
 *   │ 2              │ 标液核查       │ 无特殊标志                │
 *   └─────────────────────────────────────────────────────────────┘
 *
 * nUpdateData=1: 通知上位机数据已更新, 需要读取最新结果
 *
 * 返回: 0 — 退出 AutoRun 主循环
 */
static uint8_t mod_mode_judge(void)
{
	uint16_t rw50 = pMbHoldData->RW50;

	printk("AutoRun[20]: Mode judge (RW50=%u)\n", rw50);

	/* 根据工作模式设置完成标志 */
	switch (rw50) {
	case 1: case 5: case 6: case 7: case 43:
		/* 水样测量完成: 设置主站和 MB3 完成标志 */
		pMbHoldData->nMeasureComplet = 1;
		pMb3InputData->TestSampleFlag = 1;
		break;
	case 3: case 19: case 21: case 24: case 31:
		/* 零点标定完成: 设置零点标定标志 */
		pMbHoldData->nZeroComplet = 1;
		break;
	case 20: case 22: case 25: case 32:
		/* 量程标定完成: 设置测量完成标志 */
		pMbHoldData->nMeasureComplet = 1;
		break;
	case 2:
		/* 标液核查完成: 无特殊标志, 上位机通过其他方式判断 */
		break;
	default:
		break;
	}

	pMbHoldData->nUpdateData = 1;  /* 通知上位机: 数据已更新 */
	printk("AutoRun[20]: Done, MeasureComplet=%u ZeroComplet=%u\n",
	       pMbHoldData->nMeasureComplet, pMbHoldData->nZeroComplet);
	return 0;  /* 退出 AutoRun 主循环 */
}

/* 模块 24: 进样后返回 */
static uint8_t mod_sampling_return(void)
{
	printk("AutoRun[24]: Sampling return\n");
	return MOD_REAGENT1;
}

/* 模块 30: 采样器信号等待 */
static uint8_t mod_sampler_wait(void)
{
	printk("AutoRun[30]: Wait sampler signal\n");
	return MOD_SAMPLING;
}

/* ==================== 模块函数表 ==================== */

static const autorun_module_fn modules[] = {
	[0]  = NULL,
	[1]  = mod_drain,
	[2]  = mod_zero_rinse,
	[3]  = mod_zero_level_cal,
	[4]  = mod_ext_pump,
	[5]  = mod_sampling,
	[6]  = mod_reagent1,
	[7]  = mod_heat_digest,
	[8]  = mod_distilled_water,
	[9]  = mod_reagent2,
	[10] = mod_turbidity,
	[11] = mod_reagent3,
	[12] = mod_settle_react,
	[13] = mod_read_v2,
	[14] = mod_drain_clean,
	[15] = mod_read_v1_blank,
	[16] = mod_read_v1_react,
	[17] = mod_absorbance,
	[18] = mod_finish,
	[19] = mod_calc_result,
	[20] = mod_mode_judge,
	[24] = mod_sampling_return,
	[30] = mod_sampler_wait,
};

#define MODULE_COUNT (sizeof(modules) / sizeof(modules[0]))

/* ==================== AutoRun 主循环 ==================== */

void AutoRun(void)
{
	uint8_t step = MOD_DRAIN;
	uint16_t step_count = 0;

	printk("AutoRun started\n");
	pMbHoldData->RW4229 = 0;
	pMbHoldData->RW4230 = 0;

	uint8_t last_step = 0;

	while (step > 0 && step < MODULE_COUNT && step_count < 1000) {
		if (step != last_step) {
			pMbHoldData->RW4229 = step;  /* 仅在模块切换时更新 */
			last_step = step;
		}

		autorun_module_fn fn = modules[step];
		if (!fn) {
			printk("AutoRun: invalid step %u\n", step);
			break;
		}

		step = fn();
		step_count++;

		k_sleep(K_MSEC(10));
	}

	pMbHoldData->RW4229 = 0;
	printk("AutoRun finished (%u steps)\n", step_count);
}
