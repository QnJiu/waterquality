/*
 * main.c — nanoWAys 总磷水质分析仪 生产固件入口
 *
 * 启动流程:
 *   1. main() 初始化 BSP 外设 (阀门/泵/ADC/PWM/DAC/Flash)
 *   2. 从 Flash 加载用户配置 (Modbus 地址/波特率)
 *   3. K_THREAD_DEFINE 创建 6 个业务任务 (编译时静态分配栈)
 *   4. main() 进入心跳 LED 循环 (1s 翻转)
 *
 * 任务架构:
 *   ┌─────────────────────┬──────┬──────────┬──────────────────────────┐
 *   │ 任务                │ 优先级│ 栈大小   │ 功能                     │
 *   ├─────────────────────┼──────┼──────────┼──────────────────────────┤
 *   │ task_modbus_usart1  │  2   │ 2048 B   │ USART1 Modbus RTU 从站   │
 *   │ task_modbus_usart3  │  2   │ 2048 B   │ USART3 Modbus RTU 从站   │
 *   │ task_modbus_usart6  │  2   │ 2048 B   │ USART6 Modbus RTU 从站   │
 *   │ task_process_control│  2   │ 4096 B   │ 主分析流程 (AutoRun)     │
 *   │ task_error_manage   │  3   │ 2048 B   │ 错误处理 + 安全停机      │
 *   │ task_stir_ctrl      │  2   │ 1024 B   │ 搅拌控制 + 数字输入消抖  │
 *   └─────────────────────┴──────┴──────────┴──────────────────────────┘
 *
 * 内存布局:
 *   FLASH: ~53 KB / 512 KB (10.16%) — 固件代码 + 常量
 *   RAM:   ~36 KB / 128 KB (27.91%) — 全局变量 + 任务栈 + Modbus 缓冲区
 *   CCM:   未使用 (64 KB 可用, 未来可放栈或数据)
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "bsp/bsp_gpio_map.h"
#include "bsp/bsp_valve.h"
#include "bsp/bsp_pump.h"
#include "bsp/bsp_adc_ads1220.h"
#include "bsp/bsp_adc_internal.h"
#include "bsp/bsp_pwm.h"
#include "bsp/bsp_dac.h"
#include "bsp/bsp_storage.h"
#include "tasks/task_modbus.h"
#include "tasks/task_process_control.h"
#include "tasks/task_error_manage.h"
#include "tasks/task_stir_ctrl.h"

/* GPIO 设备规格 (设备树定义) */
static const struct gpio_dt_spec led1 = LED1_GPIO;    /* 心跳 LED */
static const struct gpio_dt_spec buzzer = BUZZER_GPIO; /* 蜂鸣器 (开机提示) */

/* Flash 用户数据 — 全局分配避免栈溢出 (USER_DATA_t ~2092 bytes) */
static USER_DATA_t user_data;

/* ==================== 业务任务定义 (编译时静态创建) ==================== */

/* Modbus 从站任务: USART1/3/6 各一个, 优先级 2 */
K_THREAD_DEFINE(mb1_tid, 2048, task_modbus_usart1, NULL, NULL, NULL, 2, 0, 0);
K_THREAD_DEFINE(mb3_tid, 2048, task_modbus_usart3, NULL, NULL, NULL, 2, 0, 0);
K_THREAD_DEFINE(mb6_tid, 2048, task_modbus_usart6, NULL, NULL, NULL, 2, 0, 0);

/* 主分析流程任务: 优先级 2, 栈 4096 (AutoRun 含 PID 控温, 栈需求较大) */
K_THREAD_DEFINE(pc_tid, 4096, task_process_control, NULL, NULL, NULL, 2, 0, 0);

/* 错误处理任务: 优先级 3 (高于业务任务, 确保安全停机及时响应) */
K_THREAD_DEFINE(err_tid, 2048, task_error_manage, NULL, NULL, NULL, 3, 0, 0);

/* 搅拌控制任务: 优先级 2, 栈 1024 (轻量级 10ms 轮询) */
K_THREAD_DEFINE(stir_tid, 1024, task_stir_ctrl, NULL, NULL, NULL, 2, 0, 0);

/* ==================== main() ==================== */

int main(void)
{
	printk("\n=== nanoWAys Production Firmware ===\n\n");

	/* GPIO 初始化: LED + 蜂鸣器 (默认关闭) */
	gpio_pin_configure_dt(&led1, GPIO_OUTPUT_INACTIVE);
	gpio_pin_configure_dt(&buzzer, GPIO_OUTPUT_INACTIVE);

	/* BSP 外设初始化 (按依赖顺序) */
	bsp_valve_init();         /* 19 路阀门 GPIO (Y1-Y19) */
	bsp_pump_init();          /* 蠕动泵 GPIO (DIR/EN) */
	bsp_ads1220_init();       /* SPI2 + ADS1220 外部 24-bit ADC */
	bsp_ads1220_config();     /* ADS1220 寄存器配置 (采样率/增益/通道) */
	bsp_adc_internal_init();  /* ADC1 内部 6 通道 (液位/温度/电源) */
	bsp_pwm_init();           /* TIM2/3/4 PWM (电机/加热/搅拌) */
	bsp_dac_init();           /* DAC PA4/PA5 (模拟输出) */

	/* Flash 存储: 加载用户配置 (Modbus 地址/波特率) */
	bsp_storage_init(&user_data);
	pMbHoldData->nLocalAddr = user_data.local_addr;  /* 恢复 Modbus 地址 */
	pMbHoldData->nBand = user_data.band;              /* 恢复波特率 */

	/* 开机提示: 蜂鸣器短响 100ms */
	gpio_pin_set_dt(&buzzer, 1);
	k_sleep(K_MSEC(100));
	gpio_pin_set_dt(&buzzer, 0);

	printk("Boot OK. Addr=%u Band=%u\n",
	       user_data.local_addr, user_data.band);

	/* 心跳 LED: 1s 翻转, 确认系统存活 */
	while (1) {
		gpio_pin_toggle_dt(&led1);
		k_sleep(K_MSEC(1000));
	}
	return 0;  /* 不可达 */
}
