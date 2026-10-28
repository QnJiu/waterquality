/*
 * bsp_storage.h — Flash 用户数据持久化
 *
 * 使用 Zephyr Flash API (flash_write/flash_erase/flash_read)
 * 存储区: STM32 Sector 5 (0x08020000, 128KB)
 */

#ifndef BSP_STORAGE_H
#define BSP_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

#define STORAGE_MAGIC  0x55AA55AA  /* 数据有效标志 */

/*
 * USER_DATA_t — 需要掉电保存的用户数据
 */
typedef struct {
	uint32_t magic;                 /* STORAGE_MAGIC 表示数据已保存 */
	uint16_t holding_regs[512];     /* Modbus 保持寄存器快照 */
	uint16_t input_regs[512];       /* Modbus 输入寄存器快照 */
	uint8_t  coils[8];              /* 线圈状态 (64 bits) */
	uint16_t stepper_motor_speed;   /* 步进电机速度 */
	uint16_t local_addr;            /* Modbus 从站地址 */
	uint16_t band;                  /* 波特率/100 */
	float    pid_kp;                /* PID Kp */
	float    pid_ki;                /* PID Ki */
	float    pid_kd;                /* PID Kd */
	uint16_t pid_target;            /* PID 目标温度 */
	uint16_t stir_work_time;        /* 搅拌工作时间 */
	uint16_t stir_sleep_time;       /* 搅拌停止时间 */
	uint32_t checksum;              /* 简单校验和 */
} USER_DATA_t;

/**
 * @brief 初始化存储: 从 Flash 加载数据, 校验 magic + checksum
 * @param data 输出: 加载的数据; 无有效数据时填充默认值
 * @note  默认值: addr=1, band=1152(115200), motor=600, stir=30/700, pid=1000/600/1000/120
 */
void bsp_storage_init(USER_DATA_t *data);

/**
 * @brief 保存数据到 Flash (擦除 + 写入 + 校验和)
 * @param data 要保存的数据 (函数内部计算 checksum)
 * @return true=成功, false=失败
 * @note  会擦除整个存储区 (64KB), 写入前自动计算校验和
 */
bool bsp_storage_save(const USER_DATA_t *data);

/**
 * @brief 从 Flash 读取数据 (不校验, 直接读取)
 * @param data 输出: 读取的数据
 * @return true=成功, false=Flash 设备未就绪
 */
bool bsp_storage_read(USER_DATA_t *data);

/** @brief 擦除整个存储区 (64KB) */
bool bsp_storage_erase(void);

#endif /* BSP_STORAGE_H */
