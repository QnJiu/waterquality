/*
 * bsp_adc_ads1220.h — ADS1220 24-bit Σ-Δ ADC 驱动
 *
 * 硬件连接:
 *   SPI2: PB13(SCK), PB14(MISO), PB15(MOSI)
 *   CS:   PB12 (active-low, 软件控制)
 *   DRDY: PE14 (输入, 下降沿表示数据就绪)
 *
 * 用途: 比色皿光谱测量 (V1/V2/V3), 液位 AD 采集
 *
 * 工作模式: 连续转换, 1000SPS, 50Hz 抑制
 *   bsp_ads1220_config() 配置后自动开始连续转换
 *   bsp_ads1220_read_channels() 切换通道 + 等待 DRDY + 读取
 *
 * 寄存器/命令宏定义保留原值 (来自 TI ADS1220 数据手册)
 */

#ifndef BSP_ADC_ADS1220_H
#define BSP_ADC_ADS1220_H

#include <stdint.h>
#include <stdbool.h>

#define ADS1220_NO_ERROR        0
#define ADS1220_ERROR          -1

/* 命令 */
#define ADS1220_CMD_RDATA    0x10
#define ADS1220_CMD_RREG     0x20
#define ADS1220_CMD_WREG     0x40
#define ADS1220_CMD_SYNC     0x08
#define ADS1220_CMD_SHUTDOWN 0x02
#define ADS1220_CMD_RESET    0x06

/* 寄存器地址 */
#define ADS1220_0_REGISTER   0x00
#define ADS1220_1_REGISTER   0x01
#define ADS1220_2_REGISTER   0x02
#define ADS1220_3_REGISTER   0x03

/* Reg 0: MUX */
#define ADS1220_MUX_0_1    0x00
#define ADS1220_MUX_0_2    0x10
#define ADS1220_MUX_0_3    0x20
#define ADS1220_MUX_1_2    0x30
#define ADS1220_MUX_1_3    0x40
#define ADS1220_MUX_2_3    0x50
#define ADS1220_MUX_1_0    0x60
#define ADS1220_MUX_3_2    0x70
#define ADS1220_MUX_0_G    0x80
#define ADS1220_MUX_1_G    0x90
#define ADS1220_MUX_2_G    0xa0
#define ADS1220_MUX_3_G    0xb0
#define ADS1220_MUX_EX_VREF 0xc0
#define ADS1220_MUX_AVDD   0xd0
#define ADS1220_MUX_DIV2   0xe0

/* Reg 0: GAIN */
#define ADS1220_GAIN_1   0x00
#define ADS1220_GAIN_2   0x02
#define ADS1220_GAIN_4   0x04
#define ADS1220_GAIN_8   0x06
#define ADS1220_GAIN_16  0x08
#define ADS1220_GAIN_32  0x0a
#define ADS1220_GAIN_64  0x0c
#define ADS1220_GAIN_128 0x0e

/* Reg 0: PGA_BYPASS */
#define ADS1220_PGA_BYPASS   0x01

/* Reg 1: Data Rate */
#define ADS1220_DR_20    0x00
#define ADS1220_DR_45    0x20
#define ADS1220_DR_90    0x40
#define ADS1220_DR_175   0x60
#define ADS1220_DR_330   0x80
#define ADS1220_DR_600   0xa0
#define ADS1220_DR_1000  0xc0

/* Reg 1: Mode */
#define ADS1220_MODE_NORMAL 0x00
#define ADS1220_MODE_DUTY   0x08
#define ADS1220_MODE_TURBO  0x10
#define ADS1220_MODE_DCT    0x18

/* Reg 1: Conversion Mode / Temp / Burnout */
#define ADS1220_CC            0x04
#define ADS1220_TEMP_SENSOR   0x02
#define ADS1220_BCS           0x01

/* Reg 2: VREF */
#define ADS1220_VREF_INT     0x00
#define ADS1220_VREF_EX_DED  0x40
#define ADS1220_VREF_EX_AIN  0x80
#define ADS1220_VREF_SUPPLY  0xc0

/* Reg 2: Filter */
#define ADS1220_REJECT_OFF   0x00
#define ADS1220_REJECT_BOTH  0x10
#define ADS1220_REJECT_50    0x20
#define ADS1220_REJECT_60    0x30

/* Reg 2: PSW / IDAC */
#define ADS1220_PSW_SW       0x08
#define ADS1220_IDAC_OFF     0x00
#define ADS1220_IDAC_10      0x01
#define ADS1220_IDAC_50      0x02
#define ADS1220_IDAC_100     0x03
#define ADS1220_IDAC_250     0x04
#define ADS1220_IDAC_500     0x05
#define ADS1220_IDAC_1000    0x06
#define ADS1220_IDAC_2000    0x07

/* Reg 3: IDAC routing / DRDYM */
#define ADS1220_IDAC1_OFF    0x00
#define ADS1220_IDAC1_AIN0   0x20
#define ADS1220_IDAC1_AIN1   0x40
#define ADS1220_IDAC1_AIN2   0x60
#define ADS1220_IDAC1_AIN3   0x80
#define ADS1220_IDAC1_REFP0  0xa0
#define ADS1220_IDAC1_REFN0  0xc0
#define ADS1220_IDAC2_OFF    0x00
#define ADS1220_IDAC2_AIN0   0x04
#define ADS1220_IDAC2_AIN1   0x08
#define ADS1220_IDAC2_AIN2   0x0c
#define ADS1220_IDAC2_AIN3   0x10
#define ADS1220_IDAC2_REFP0  0x14
#define ADS1220_IDAC2_REFN0  0x18
#define ADS1220_DRDY_MODE    0x02

/* ==================== 函数声明 ==================== */

/** @brief 初始化 SPI2 + CS + DRDY 引脚, 配置 SPI Mode 1 10MHz */
int  bsp_ads1220_init(void);

/** @brief 配置 ADS1220 寄存器: 1000SPS, 连续转换, 50Hz 抑制, 启动转换 */
void bsp_ads1220_config(void);

/* 底层 SPI */

/** @brief CS 控制 (active-low): true=拉低选中, false=拉高释放 */
void bsp_ads1220_cs(bool assert);

/** @brief SPI 发送单字节 */
void bsp_ads1220_send_byte(uint8_t data);

/** @brief SPI 收发单字节 (发送 tx_data, 返回接收到的数据) */
uint8_t bsp_ads1220_recv_byte(uint8_t tx_data);

/* 中层 */

/** @brief 读取 24-bit ADC 数据 (RDATA 命令), 返回有符号 32-bit 值 */
int32_t bsp_ads1220_read_data(void);

/** @brief 读取连续寄存器 (RREG 命令) */
void bsp_ads1220_read_regs(int addr, int num, unsigned int *data);

/** @brief 写入连续寄存器 (WREG 命令) */
void bsp_ads1220_write_regs(int addr, int num, unsigned int *data);

/* 单字节命令 */

/** @brief 发送 SYNC 命令 (启动/同步转换) */
void bsp_ads1220_send_start(void);

/** @brief 发送 RESET 命令 */
void bsp_ads1220_send_reset(void);

/** @brief 发送 SHUTDOWN 命令 (低功耗模式) */
void bsp_ads1220_send_shutdown(void);

/* 寄存器字段 setter (修改 reg_cache 并写入硬件) */

int bsp_ads1220_set_channel(int mux);    /* Reg0[7:4]: 输入多路复用 */
int bsp_ads1220_set_gain(int gain);      /* Reg0[3:1]: PGA 增益 */
int bsp_ads1220_set_pga_bypass(int bypass); /* Reg0[0]: PGA 旁路 */
int bsp_ads1220_set_data_rate(int rate); /* Reg1[7:5]: 数据速率 */
int bsp_ads1220_set_mode(int mode);      /* Reg1[4:3]: 工作模式 */
int bsp_ads1220_set_conv_mode(int cm);   /* Reg1[2]: 转换模式 */
int bsp_ads1220_set_vref(int vref);      /* Reg2[7:6]: 参考电压源 */
int bsp_ads1220_set_rejection(int rej);  /* Reg2[5:4]: 50/60Hz 抑制 */
int bsp_ads1220_set_idac(int idac);      /* Reg2[2:0]: IDAC 电流 */
int bsp_ads1220_set_psw(int psw);        /* Reg2[3]: 低侧电源开关 */
int bsp_ads1220_set_drdym(int drdym);    /* Reg3[1]: DRDY 模式 */

/**
 * @brief 读取双通道 ADC 值 (交替采集 CH2/CH3)
 * @param pAD  输出: pAD[0]=CH3 均值, pAD[1]=CH2 均值 (右移 8 位)
 * @param count 采样次数 (每次交替读 CH3+CH2)
 *
 * 工作流程: 切 CH3 → 等 DRDY → 读 → 切 CH2 → 等 DRDY → 读 → 循环
 */
void bsp_ads1220_read_channels(uint16_t *pAD, uint16_t count);

#endif /* BSP_ADC_ADS1220_H */
