/*
 * bsp_adc_ads1220.c — ADS1220 24-bit Σ-Δ ADC 驱动实现
 *
 * 注意: CS 由软件手动控制 (不使用 Zephyr SPI 自动 CS),
 *       因为 ADS1220 的时序需要在 CS 拉低后发送命令字节
 */

#include "bsp/bsp_adc_ads1220.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

/* SPI 设备 (raw, 不用 SPI_DT_SPEC_GET 因为手动 CS) */
static const struct device *spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi2));

/* CS (PB12, active-low) */
static const struct gpio_dt_spec cs =
	GPIO_DT_SPEC_GET(DT_NODELABEL(spi2), cs_gpios);

/* DRDY (PE14, input+pull-up, raw GPIO API) */
static const struct device *gpioe_dev = DEVICE_DT_GET(DT_NODELABEL(gpioe));

/* SPI 配置 */
static struct spi_config spi_cfg;

/* 单 buffer 发送/接收辅助 */
#define SINGLE_BUF_SET(p) (&(const struct spi_buf_set){ .buffers = p, .count = 1 })

/* 通道 MUX 预定义值 */
static const uint8_t channel_mux[4] = {
	ADS1220_MUX_0_G, ADS1220_MUX_1_G, ADS1220_MUX_2_G, ADS1220_MUX_3_G
};

/* 内部变量 */
static uint32_t ad_read_buf[4][128];
static int ad_samples;

/* ==================== 初始化 ==================== */

int bsp_ads1220_init(void)
{
	if (!device_is_ready(spi_dev)) {
		printk("SPI2 not ready for ADS1220\n");
		return -1;
	}

	if (!gpio_is_ready_dt(&cs)) {
		printk("ADS1220 CS GPIO not ready\n");
		return -1;
	}
	gpio_pin_configure_dt(&cs, GPIO_OUTPUT_INACTIVE);  /* CS HIGH = disabled */

	/* DRDY 输入 (PE14, 上拉) */
	gpio_pin_configure(gpioe_dev, 14, GPIO_INPUT | GPIO_PULL_UP);

	/* SPI 配置 (CPOL=0, CPHA=1 = SPI Mode 1, 8-bit, MSB) */
	spi_cfg.frequency = 10000000;
	spi_cfg.operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) |
			    SPI_TRANSFER_MSB | SPI_MODE_CPHA;
	spi_cfg.slave = 0;
	spi_cfg.cs = (struct spi_cs_control){ .gpio = GPIO_DT_SPEC_GET(DT_NODELABEL(spi2), cs_gpios), .delay = 0 };

	printk("ADS1220 initialized: SPI2 10MHz, CS=PB12, DRDY=PE14\n");
	return 0;
}

/* ==================== CS 和 DRDY ==================== */

void bsp_ads1220_cs(bool assert)
{
	gpio_pin_set_dt(&cs, assert ? 1 : 0);  /* active-low: 1=assert */
}

/* ==================== 底层 SPI ==================== */

void bsp_ads1220_send_byte(uint8_t data)
{
	struct spi_buf tx = { .buf = &data, .len = 1 };
	spi_write(spi_dev, &spi_cfg, SINGLE_BUF_SET(&tx));
}

uint8_t bsp_ads1220_recv_byte(uint8_t tx_data)
{
	uint8_t rx;
	struct spi_buf tx = { .buf = &tx_data, .len = 1 };
	struct spi_buf rx_buf = { .buf = &rx, .len = 1 };
	spi_transceive(spi_dev, &spi_cfg, SINGLE_BUF_SET(&tx), SINGLE_BUF_SET(&rx_buf));
	return rx;
}

/* ==================== 读取数据 ==================== */

int32_t bsp_ads1220_read_data(void)
{
	int32_t data;

	bsp_ads1220_cs(true);
	bsp_ads1220_send_byte(ADS1220_CMD_RDATA);

	data  = (int32_t)bsp_ads1220_recv_byte(0xFF) << 16;
	data |= (int32_t)bsp_ads1220_recv_byte(0xFF) << 8;
	data |= (int32_t)bsp_ads1220_recv_byte(0xFF);

	if (data & 0x800000)
		data |= 0xff000000;

	bsp_ads1220_cs(false);
	return data;
}

/* ==================== 寄存器读写 ==================== */

void bsp_ads1220_read_regs(int addr, int num, unsigned int *data)
{
	uint8_t cmd = ADS1220_CMD_RREG | ((addr << 2) & 0x0C) | ((num - 1) & 0x03);

	bsp_ads1220_cs(true);
	bsp_ads1220_send_byte(cmd);

	for (int i = 0; i < num; i++)
		data[i] = bsp_ads1220_recv_byte(0xFF);

	bsp_ads1220_cs(false);
}

void bsp_ads1220_write_regs(int addr, int num, unsigned int *data)
{
	uint8_t cmd = ADS1220_CMD_WREG | ((addr << 2) & 0x0C) | ((num - 1) & 0x03);

	bsp_ads1220_cs(true);
	bsp_ads1220_send_byte(cmd);

	for (int i = 0; i < num; i++)
		bsp_ads1220_send_byte((uint8_t)data[i]);

	bsp_ads1220_cs(false);
}

/* ==================== 单字节命令 ==================== */

static void send_command(uint8_t cmd)
{
	bsp_ads1220_cs(true);
	bsp_ads1220_send_byte(cmd);
	bsp_ads1220_cs(false);
}

void bsp_ads1220_send_start(void)   { send_command(ADS1220_CMD_SYNC); }
void bsp_ads1220_send_reset(void)   { send_command(ADS1220_CMD_RESET); }
void bsp_ads1220_send_shutdown(void) { send_command(ADS1220_CMD_SHUTDOWN); }

/* ==================== 寄存器字段 getter/setter ==================== */

static unsigned int reg_cache[4];

static void reg_write_field(int reg, unsigned int value, unsigned int mask, unsigned int shift)
{
	unsigned int val = reg_cache[reg];
	val &= ~mask;
	val |= (value << shift) & mask;
	reg_cache[reg] = val;
	bsp_ads1220_write_regs(reg, 1, &val);
}

int bsp_ads1220_set_channel(int mux)  { reg_write_field(0, mux, 0xF0, 4); return 0; }
int bsp_ads1220_set_gain(int gain)    { reg_write_field(0, gain, 0x0E, 1); return 0; }
int bsp_ads1220_set_pga_bypass(int bp) { reg_write_field(0, bp, 0x01, 0); return 0; }
int bsp_ads1220_set_data_rate(int dr) { reg_write_field(1, dr, 0xE0, 5); return 0; }
int bsp_ads1220_set_mode(int mode)    { reg_write_field(1, mode, 0x18, 3); return 0; }
int bsp_ads1220_set_conv_mode(int cm)  { reg_write_field(1, cm, 0x04, 2); return 0; }
int bsp_ads1220_set_vref(int vref)    { reg_write_field(2, vref, 0xC0, 6); return 0; }
int bsp_ads1220_set_rejection(int r)  { reg_write_field(2, r, 0x30, 4); return 0; }
int bsp_ads1220_set_idac(int idac)    { reg_write_field(2, idac, 0x07, 0); return 0; }
int bsp_ads1220_set_psw(int psw)      { reg_write_field(2, psw, 0x08, 3); return 0; }
int bsp_ads1220_set_drdym(int drdym)  { reg_write_field(3, drdym, 0x02, 1); return 0; }

/* ==================== 配置 ==================== */

void bsp_ads1220_config(void)
{
	unsigned int val;

	/* Reg 0: MUX=AIN3_G, PGA_BYPASS=1 */
	val = ADS1220_MUX_3_G | ADS1220_PGA_BYPASS;
	bsp_ads1220_write_regs(ADS1220_0_REGISTER, 1, &val);
	reg_cache[0] = val;
	bsp_ads1220_read_regs(ADS1220_0_REGISTER, 1, &val);

	/* Reg 1: 1000 SPS, Continuous Conversion, Normal mode */
	val = ADS1220_DR_1000 | ADS1220_CC | ADS1220_MODE_NORMAL;
	bsp_ads1220_write_regs(ADS1220_1_REGISTER, 1, &val);
	reg_cache[1] = val;
	bsp_ads1220_read_regs(ADS1220_1_REGISTER, 1, &val);

	/* Reg 2: 50Hz rejection, Internal VREF */
	val = ADS1220_REJECT_50;
	bsp_ads1220_write_regs(ADS1220_2_REGISTER, 1, &val);
	reg_cache[2] = val;
	bsp_ads1220_read_regs(ADS1220_2_REGISTER, 1, &val);

	/* 启动连续转换 */
	bsp_ads1220_send_start();

	printk("ADS1220 configured: 1000SPS, continuous, 50Hz reject\n");
}

/* ==================== 单通道设置 ==================== */

static void set_single_channel(int ch)
{
	if (ch > 3) ch = 0;
	unsigned int val = 0x01 | channel_mux[ch];
	bsp_ads1220_set_channel(val);
}

/*
 * 高层: 交替读取 CH2/CH3 双通道
 *
 * AutoRun 使用: CH3 用于光谱测量 (V1/V2/V3), CH2 用于液位检测
 *
 * 工作流程 (每次循环):
 *   1. 设置 MUX=CH3 → 等待 DRDY → 读取 CH3 值
 *   2. 设置 MUX=CH2 → 等待 DRDY → 读取 CH2 值
 *   3. 累加到 res2/res3, 循环 count 次
 *   4. 返回平均值 (右移 8 位, 保留 16-bit 有效位)
 */
void bsp_ads1220_read_channels(uint16_t *pAD, uint16_t count)
{
	int res2 = 0, res3 = 0;
	int samples = 0, timeout;

	set_single_channel(3);  /* 先切到 CH3 */

	while (samples < (int)count) {
		/* 等待 DRDY = LOW (PE14 下降沿表示数据就绪) */
		timeout = 10000;
		while (gpio_pin_get(gpioe_dev, 14) && --timeout >= 0) {}

		int reading = (int)bsp_ads1220_read_data();

		/* 切换到通道 2 */
		set_single_channel(2);

		res3 += reading;
		ad_read_buf[3][ad_samples] = (uint32_t)reading;

		/* 等待 DRDY */
		timeout = 10000;
		while (gpio_pin_get(gpioe_dev, 14) && --timeout >= 0) {}

		reading = (int)bsp_ads1220_read_data();

		/* 切回通道 3 */
		set_single_channel(3);

		res2 += reading;
		ad_read_buf[2][ad_samples] = (uint32_t)reading;

		ad_samples = (ad_samples + 1) & 0x7F;  /* 环形缓冲区索引 */
		samples++;
	}

	if (count > 0) {
		/* 右移 8 位: 24-bit → 16-bit 有效位, 丢弃低 8 位噪声 */
		pAD[0] = (uint16_t)((res3 / count) >> 8);  /* CH3 */
		pAD[1] = (uint16_t)((res2 / count) >> 8);  /* CH2 */
	}
}
