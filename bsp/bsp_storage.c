/*
 * bsp_storage.c — Flash 用户数据持久化
 *
 * 使用 Zephyr Flash API:
 *   flash_read()  — 从 Flash 读取数据
 *   flash_write() — 向 Flash 写入数据 (需先擦除)
 *   flash_erase() — 擦除 Flash 扇区
 *
 * 存储区: STM32 Sector 5 (0x08020000, 128KB)
 */

#include "bsp_storage.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/printk.h>
#include <string.h>

/* 存储分区地址 (与 DTS storage_partition 对应) */
/* 注意: Zephyr flash API 使用相对于 flash 基地址的偏移, 不是绝对地址 */
#define STORAGE_OFFSET  0x10000  /* Sector 4: 0x08010000 - 0x08000000 = 64KB */
#define STORAGE_SIZE    0x10000  /* 64KB */

static const struct device *flash_dev;

static uint32_t calc_checksum(const USER_DATA_t *data)
{
	uint32_t sum = 0;
	const uint8_t *p = (const uint8_t *)data;
	/* 校验除 checksum 字段外的所有数据 */
	for (size_t i = 0; i < offsetof(USER_DATA_t, checksum); i++) {
		sum += p[i];
	}
	return sum;
}

void bsp_storage_init(USER_DATA_t *data)
{
	flash_dev = DEVICE_DT_GET(DT_NODELABEL(flash));
	if (!device_is_ready(flash_dev)) {
		printk("Storage: flash device not ready\n");
		memset(data, 0, sizeof(USER_DATA_t));
		return;
	}

	printk("Storage: flash device ready\n");

	/* 尝试从 Flash 加载 */
	if (bsp_storage_read(data)) {
		if (data->magic == STORAGE_MAGIC) {
			uint32_t expected = calc_checksum(data);
			if (data->checksum == expected) {
				printk("Storage: valid data loaded from flash\n");
				return;
			}
			printk("Storage: checksum mismatch (got 0x%08X, expected 0x%08X)\n",
			       data->checksum, expected);
		} else {
			printk("Storage: no valid data (magic=0x%08X)\n", data->magic);
		}
	} else {
		printk("Storage: flash read failed\n");
	}

	/* 无有效数据, 使用默认值 */
	memset(data, 0, sizeof(USER_DATA_t));
	data->magic = STORAGE_MAGIC;
	data->local_addr = 1;
	data->band = 1152;        /* 115200 baud */
	data->stepper_motor_speed = 600;
	data->stir_work_time = 30;
	data->stir_sleep_time = 700;
	data->pid_kp = 1000.0f;
	data->pid_ki = 600.0f;
	data->pid_kd = 1000.0f;
	data->pid_target = 120;
	data->checksum = calc_checksum(data);

	printk("Storage: using default values\n");
}

bool bsp_storage_save(const USER_DATA_t *data)
{
	if (!flash_dev || !device_is_ready(flash_dev)) {
		printk("Storage: flash not ready for write\n");
		return false;
	}

	/* 计算校验和 (写入前需要去掉 const) */
	USER_DATA_t tmp;
	memcpy(&tmp, data, sizeof(USER_DATA_t));
	tmp.checksum = calc_checksum(&tmp);

	/* 擦除整个存储区 */
	int ret = flash_erase(flash_dev, STORAGE_OFFSET, STORAGE_SIZE);
	if (ret != 0) {
		printk("Storage: erase failed (%d)\n", ret);
		return false;
	}

	/* 写入数据 */
	ret = flash_write(flash_dev, STORAGE_OFFSET, &tmp, sizeof(USER_DATA_t));
	if (ret != 0) {
		printk("Storage: write failed (%d)\n", ret);
		return false;
	}

	printk("Storage: saved %zu bytes to flash\n", sizeof(USER_DATA_t));
	return true;
}

bool bsp_storage_read(USER_DATA_t *data)
{
	if (!flash_dev || !device_is_ready(flash_dev)) {
		return false;
	}

	int ret = flash_read(flash_dev, STORAGE_OFFSET, data, sizeof(USER_DATA_t));
	return (ret == 0);
}

bool bsp_storage_erase(void)
{
	if (!flash_dev || !device_is_ready(flash_dev)) {
		return false;
	}

	int ret = flash_erase(flash_dev, STORAGE_OFFSET, STORAGE_SIZE);
	if (ret != 0) {
		printk("Storage: erase failed (%d)\n", ret);
		return false;
	}

	printk("Storage: erased %d bytes\n", STORAGE_SIZE);
	return true;
}
