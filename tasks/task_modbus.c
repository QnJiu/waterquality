/*
 * task_modbus.c — Modbus 寄存器回调 + 3 路从站任务线程
 *
 * ==================== 架构概述 ====================
 *
 * 本文件实现 Modbus RTU 从站的核心逻辑:
 *   1. 寄存器回调函数 — 响应主站的读/写请求
 *   2. 3 个从站任务线程 — USART1/3/6 各一个, 处理 Modbus 帧
 *
 * 三路 Modbus 的区别:
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │ 总线   │ 地址   │ 波特率   │ 缓冲区     │ 用途            │
 *   ├─────────────────────────────────────────────────────────────┤
 *   │ USART1 │ 可配置 │ 可配置   │ 主缓冲区   │ 上位机通信(主站)│
 *   │ USART3 │ 101    │ 115200   │ MB3缓冲区  │ 管控仪通信      │
 *   │ USART6 │ 可配置 │ 可配置   │ 主缓冲区   │ 扩展通信        │
 *   └─────────────────────────────────────────────────────────────┘
 *
 * ==================== 寄存器映射 ====================
 *
 *   保持寄存器 (4X, 功能码 03/06/16): pMbHoldData (读写)
 *     — 上位机可读写的配置参数和控制命令
 *     — 例如: nHostCMD(PDU 0), RW50(工作模式), nAutoRun(PDU 130)
 *
 *   输入寄存器 (3X, 功能码 04): pMbInputData (只读)
 *     — 仪器状态和测量结果
 *     — 例如: nFunErrorCode(错误码), nFirmVersion(固件版本)
 *
 *   线圈 (0X, 功能码 01/05/15): pMbCoilData (64 bits)
 *     — 开关量控制(预留)
 *
 * ==================== PDU 地址约定 (重要!) ====================
 *
 *   FreeModbus 核心在调用回调前会执行 usRegAddress++,
 *   所以 PDU 地址 = 缓冲区数组索引 (不是索引+1!)
 *
 *   例如: nAutoRun 在 buffer[130] → PDU 地址 = 130 (0x82)
 *   上位机发送: 01 06 00 82 00 01 [CRC] → 写入 nAutoRun=1
 *
 * ==================== 并发安全 ====================
 *
 *   三路 Modbus 共享 pMbHoldData/pMbInputData 缓冲区,
 *   通过 ModbusBusMutex 互斥锁保证并发安全:
 *     任务循环: k_sem_take → k_mutex_lock → eMBPoll → k_mutex_unlock
 */

#include "task_modbus.h"
#include "task_process_control.h"
#include "hostcmd.h"
#include "bsp/bsp_valve.h"
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>

/* FreeModbus API */
#include "mb.h"
#include "mbutils.h"
#include "Modbus.h"

/* port 层提供的信号量 (用于唤醒 Modbus 任务) */
extern struct k_sem mb1_event_sem;
extern struct k_sem mb3_event_sem;
extern struct k_sem mb6_event_sem;
extern struct k_mutex ModbusBusMutex;

/* ==================== 寄存器缓冲区 ==================== */

/*
 * 缓冲区布局:
 *   主缓冲区 (_mb_*): USART1 和 USART6 共享
 *   MB3 缓冲区 (_mb3_*): USART3 独立使用 (管控仪专用)
 *
 * 寄存器数量: REG_HOLDING_NREGS=256, REG_INPUT_NREGS=256
 * 线圈: COIL_NCOILS=64 (8 bytes), 离散输入: DISCRETE_INPUT_NDISCRETES=64
 */

#define FIRMWARE_VERSION 24088  /* 固件版本号 (上位机可读) */

/* 主缓冲区 (USART1/6 共享) */
static uint16_t _mb_holding_buf[REG_HOLDING_NREGS];   /* 保持寄存器: 512 bytes */
static uint16_t _mb_input_buf[REG_INPUT_NREGS];       /* 输入寄存器: 512 bytes */
static uint8_t  _mb_coil_buf[COIL_NCOILS / 8];        /* 线圈: 8 bytes */
static uint8_t  _mb_discrete_buf[DISCRETE_INPUT_NDISCRETES / 8]; /* 离散输入: 8 bytes */

/* MB3 缓冲区 (USART3 独立) */
static uint16_t _mb3_holding_buf[REG_HOLDING_NREGS];  /* 保持寄存器: 512 bytes */
static uint16_t _mb3_input_buf[REG_INPUT_NREGS];      /* 输入寄存器: 512 bytes */

/*
 * 全局数据指针 — 通过结构体指针访问缓冲区, 语义清晰:
 *   pMbHoldData->nAutoRun  等价于  _mb_holding_buf[130]
 *   pMbInputData->nFunErrorCode 等价于  _mb_input_buf[0]
 */
MODBUS_HOLDING_REG_t *pMbHoldData = (MODBUS_HOLDING_REG_t *)_mb_holding_buf;
MODBUS_INPUT_REG_t   *pMbInputData = (MODBUS_INPUT_REG_t *)_mb_input_buf;
uint8_t              *pMbCoilData  = _mb_coil_buf;

MODBUS3_HOLDING_REG_t *pMb3HoldData = (MODBUS3_HOLDING_REG_t *)_mb3_holding_buf;
MODBUS3_INPUT_REG_t   *pMb3InputData = (MODBUS3_INPUT_REG_t *)_mb3_input_buf;

/* Modbus 栈使用的原始缓冲区指针 (FreeModbus 核心需要) */
uint16_t *usRegHoldingBuf = _mb_holding_buf;
uint16_t *usRegInputBuf   = _mb_input_buf;
uint8_t  *usCoilBuf       = _mb_coil_buf;
uint8_t  *usDiscreteInputBuf = _mb_discrete_buf;

uint16_t *usRegHoldingBuf3 = _mb3_holding_buf;
uint16_t *usRegInputBuf3   = _mb3_input_buf;

/* ==================== 全局标志 ==================== */

volatile bool HaveNewCMDFlag;    /* 上位机写入 nHostCMD 区域时置 true */
volatile uint8_t g_mb_inst;      /* 当前 Modbus 实例号 (1/3/6), 用于选择缓冲区 */
bool AnalyzerBeWorking;          /* 分析仪工作状态标志 (预留) */

/*
 * ==================== 输入寄存器回调 (功能码 04) ====================
 *
 * 当主站发送功能码 04 (读输入寄存器) 时, FreeModbus 核心调用此函数。
 * 输入寄存器是只读的, 包含仪器状态和测量结果。
 *
 * 数据流向: 缓冲区 → pucRegBuffer → Modbus 帧 → 主站
 *
 * 字节序转换: STM32 是小端(little-endian), Modbus 是大端(big-endian)
 *   所以需要将 uint16_t 拆分为高字节在前、低字节在后
 */
eMBErrorCode eMBRegInputCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs)
{
	uint16_t *pInpBuf;
	uint16_t iRegIndex;

	/* 根据当前 Modbus 实例选择缓冲区 */
	if (g_mb_inst == 3) {
		pInpBuf = usRegInputBuf3;   /* USART3: 管控仪专用缓冲区 */
	} else {
		pInpBuf = usRegInputBuf;    /* USART1/6: 主缓冲区 */
	}

	/* 地址范围检查 */
	if ((usAddress >= REG_INPUT_START) &&
	    (usAddress + usNRegs <= REG_INPUT_START + REG_INPUT_NREGS)) {
		iRegIndex = (int)(usAddress - REG_INPUT_START);
		while (usNRegs > 0) {
			/* Modbus big-endian: 高字节在前, 低字节在后 */
			*pucRegBuffer++ = (uint8_t)(pInpBuf[iRegIndex] >> 8);   /* 高字节 */
			*pucRegBuffer++ = (uint8_t)(pInpBuf[iRegIndex] & 0xFF); /* 低字节 */
			iRegIndex++;
			usNRegs--;
		}
		return MB_ENOERR;
	}
	return MB_ENOREG;  /* 地址超出范围 */
}

/*
 * ==================== 保持寄存器回调 (功能码 03/06/16) ====================
 *
 * 当主站发送功能码 03 (读保持寄存器)、06 (写单个寄存器)、16 (写多个寄存器) 时,
 * FreeModbus 核心调用此函数。
 *
 * 数据流向:
 *   读 (MB_REG_READ):  缓冲区 → pucRegBuffer → Modbus 帧 → 主站
 *   写 (MB_REG_WRITE): 主站 → Modbus 帧 → pucRegBuffer → 缓冲区
 *
 * 上位机命令触发:
 *   当写入 PDU 0-9 区域时 (nHostCMD 所在位置), 设置 HaveNewCMDFlag=true,
 *   Modbus 任务检测到标志后调用 RS232CmdProcess() 处理命令。
 *
 * PDU 地址 = buffer 索引 (FreeModbus 回调前 usRegAddress++)
 *   例如: 写 PDU 0 → buffer[0] = nHostCMD
 *         写 PDU 130 → buffer[130] = nAutoRun
 */
eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress,
			     USHORT usNRegs, eMBRegisterMode eMode)
{
	uint16_t *pHoldBuf;
	uint16_t iRegIndex;

	/* 根据当前 Modbus 实例选择缓冲区 */
	if (g_mb_inst == 3) {
		pHoldBuf = usRegHoldingBuf3;   /* USART3: 管控仪专用 */
	} else {
		pHoldBuf = usRegHoldingBuf;    /* USART1/6: 主缓冲区 */
	}

	/* 地址范围检查 */
	if ((usAddress >= REG_HOLDING_START) &&
	    (usAddress + usNRegs <= REG_HOLDING_START + REG_HOLDING_NREGS)) {
		iRegIndex = (int)(usAddress - REG_HOLDING_START);

		if (eMode == MB_REG_READ) {
			/* 读操作: 从缓冲区复制到 Modbus 帧 (big-endian) */
			while (usNRegs > 0) {
				*pucRegBuffer++ = (uint8_t)(pHoldBuf[iRegIndex] >> 8);
				*pucRegBuffer++ = (uint8_t)(pHoldBuf[iRegIndex] & 0xFF);
				iRegIndex++;
				usNRegs--;
			}
		} else {
			/*
			 * 写操作: 从 Modbus 帧复制到缓冲区 (big-endian → little-endian)
			 *
			 * 命令触发逻辑:
			 *   - 写 PDU 0 (nHostCMD): 写入值直接作为命令码
			 *   - 写 PDU 1-9 (参数寄存器): 寄存器索引号覆盖为命令码
			 *     例如: 上位机写 PDU 1 (nBand) → nHostCMD = 1 (CMD_SET_BAUD)
			 *           上位机写 PDU 2 (nLocalAddr) → nHostCMD = 2 (CMD_SET_LOCAL_ADD)
			 *     写入的值保留在参数寄存器中, 供后续处理读取
			 *
			 * 注意: 命令码始终写入 usRegHoldingBuf[0] (MB1 缓冲区),
			 *       即使命令来自 MB3, 也写入 MB1 缓冲区
			 */
			while (usNRegs > 0) {
				pHoldBuf[iRegIndex] = ((uint16_t)pucRegBuffer[0] << 8) |
				                       (uint16_t)pucRegBuffer[1];
				if (iRegIndex == 0) {
					/* 写 PDU 0: 写入值直接作为命令码 */
					HaveNewCMDFlag = true;
				} else if (iRegIndex > 0 && iRegIndex < 10) {
					/* 写 PDU 1-9: 索引号覆盖为命令码 */
					usRegHoldingBuf[0] = iRegIndex;
					HaveNewCMDFlag = true;
				}
				iRegIndex++;
				pucRegBuffer += 2;
				usNRegs--;
			}
		}
		return MB_ENOERR;
	}
	return MB_ENOREG;  /* 地址超出范围 */
}

/* ==================== 线圈回调 (功能码 01/05/15) ==================== */

/**
 * @brief 线圈读写回调
 *
 * 线圈是 1-bit 开关量, 用于阀门控制。
 * 位序: coil 1 = usCoilBuf[0] bit0, coil 2 = usCoilBuf[0] bit1, ...
 * 阀门映射: coil N → valve N (bit 0=Y1, bit 1=Y2, ...)
 *
 * 写入流程:
 *   1. xMBUtilSetBits 更新 usCoilBuf
 *   2. AnalyzerBeWorking 防重入
 *   3. k_event_post(EV_COMM_VALVE_OPT) 唤醒 ProcessControlTask
 *   4. ProcessControlTask 调用 UpdataValveState() 更新 GPIO
 */
eMBErrorCode eMBRegCoilsCB(UCHAR *pucRegBuffer, USHORT usAddress,
			   USHORT usNCoils, eMBRegisterMode eMode)
{
	int iRegIndex;
	int iRegBitIndex;
	int iNReg;

	if ((usAddress >= COIL_START) &&
	    (usAddress + usNCoils <= COIL_START + COIL_NCOILS)) {

		iRegIndex = (int)(usAddress - COIL_START) / 8;
		iRegBitIndex = (int)(usAddress - COIL_START) % 8;

		if (eMode == MB_REG_READ) {
			iNReg = usNCoils / 8 + 1;
			while (iNReg > 0) {
				*pucRegBuffer++ = xMBUtilGetBits(
					&usCoilBuf[iRegIndex++], iRegBitIndex, 8);
				iNReg--;
			}
			/* 掩码清除多余位 */
			pucRegBuffer--;
			usNCoils = usNCoils % 8;
			if (usNCoils > 0) {
				*pucRegBuffer = *pucRegBuffer << (8 - usNCoils);
				*pucRegBuffer = *pucRegBuffer >> (8 - usNCoils);
			}
		} else {
			iNReg = usNCoils / 8 + 1;
			while (iNReg > 1) {
				xMBUtilSetBits(&usCoilBuf[iRegIndex++],
					       iRegBitIndex, 8, *pucRegBuffer++);
				iNReg--;
			}
			usNCoils = usNCoils % 8;
			xMBUtilSetBits(&usCoilBuf[iRegIndex++],
				       iRegBitIndex, usNCoils, *pucRegBuffer++);

			/* 唤醒 ProcessControlTask 更新阀门 GPIO */
			if (!AnalyzerBeWorking) {
				AnalyzerBeWorking = true;
				k_event_post(&proc_events, EV_COMM_VALVE_OPT);
			}
		}
		return MB_ENOERR;
	}
	return MB_ENOREG;
}

/**
 * @brief 根据线圈缓冲区更新阀门 GPIO 状态
 *
 *   逐位解析 usCoilBuf, bit=1 开阀, bit=0 关阀。
 *
 * 线圈→阀门映射: coil bit 0 = Y1, bit 1 = Y2, ... bit 18 = Y19
 * 直接构造 32-bit mask 调用 bsp_valve_open_mask/close_mask,
 * 一次调用完成所有阀门更新。
 */
void UpdataValveState(uint8_t *pCoilBuf)
{
	uint32_t mask = 0;

	mask |= (uint32_t)pCoilBuf[0];
	mask |= (uint32_t)pCoilBuf[1] << 8;
	mask |= (uint32_t)pCoilBuf[2] << 16;

	bsp_valve_open_mask(mask);
	bsp_valve_close_mask(~mask & 0x7FFFF);  /* 19 路阀门掩码 */
}

/* ==================== 离散输入回调 (功能码 02) ==================== */

/**
 * @brief 离散输入读取回调
 *
 * 离散输入是 1-bit 只读状态, 用于传感器状态等。
 * 当前为空实现: 始终返回 MB_ENOERR (无实际数据)。
 * 后续可扩展: 返回 X1/X2 数字输入状态。
 */
eMBErrorCode eMBRegDiscreteCB(UCHAR *pucRegBuffer, USHORT usAddress,
			      USHORT usNDiscrete)
{
	if ((usAddress >= DISCRETE_INPUT_START) &&
	    (usAddress + usNDiscrete <= DISCRETE_INPUT_START + DISCRETE_INPUT_NDISCRETES)) {
		return MB_ENOERR;
	}
	return MB_ENOREG;
}

/**
 * @brief Block 寄存器回调 (预留)
 *
 * Block 寄存器用于批量读写, 当前未实现。
 * 返回 MB_ENOREG 表示不支持此功能。
 */
eMBErrorCode eMBRegBlockCB(UCHAR *pucRegBuffer, USHORT usAddress,
			   UCHAR usNRegs, eMBRegisterMode eMode)
{
	return MB_ENOREG;
}

/* RS232CmdProcess 在 hostcmd.c 中实现 */

/*
 * ==================== Modbus 从站任务 ====================
 *
 * 3 个任务结构相同, 差异:
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │ 任务              │ 地址   │ 波特率   │ 缓冲区   │ 用途    │
 *   ├─────────────────────────────────────────────────────────────┤
 *   │ task_modbus_usart1│ 可配置 │ 可配置   │ 主缓冲区 │ 上位机  │
 *   │ task_modbus_usart3│ 101    │ 115200   │ MB3缓冲区│ 管控仪  │
 *   │ task_modbus_usart6│ 可配置 │ 可配置   │ 主缓冲区 │ 扩展    │
 *   └─────────────────────────────────────────────────────────────┘
 *
 * 任务循环:
 *   1. k_sem_take(信号量, 500ms超时) — 等待 UART ISR 接收 Modbus 帧
 *   2. k_mutex_lock(互斥锁, 200ms超时) — 获取总线访问权
 *   3. eMBPoll() — 处理 Modbus 帧 (调用寄存器回调)
 *   4. 检查 HaveNewCMDFlag — 若有新命令则调用 RS232CmdProcess()
 *   5. k_mutex_unlock(互斥锁) — 释放总线
 *
 * 信号量由 UART ISR 在接收到 Modbus 帧后 give,
 * 500ms 超时保证即使无通信也能周期唤醒 (防止死锁)。
 */

/**
 * @brief USART1 Modbus 从站任务
 *
 * 主 Modbus 从站, 地址和波特率从 pMbHoldData 读取 (上位机可配置)。
 * 共享主缓冲区, 与 USART6 互斥访问。
 *
 * 初始化: eMBInit(MB_RTU, 地址, 端口号, 波特率, 无校验)
 */
void task_modbus_usart1(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1); ARG_UNUSED(arg2); ARG_UNUSED(arg3);

	printk("Modbus USART1 task started\n");
	g_mb_inst = 1;  /* 设置当前实例号, 寄存器回调据此选择缓冲区 */
	pMbInputData->nFirmVersion = FIRMWARE_VERSION;

	/* 初始化 FreeModbus: RTU 模式, 地址 1, USART1, 115200 baud, 无校验 */
	eMBInit(MB_RTU, 1, 1, 115200, MB_PAR_NONE);
	eMBEnable();

	while (1) {
		/* 等待 UART ISR 接收 Modbus 帧 (500ms 超时防死锁) */
		k_sem_take(&mb1_event_sem, K_MSEC(500));

		/* 获取总线互斥锁 (200ms 超时, 避免长时间阻塞) */
		if (k_mutex_lock(&ModbusBusMutex, K_MSEC(200)) != 0) {
			continue;  /* 获取锁失败, 跳过本轮 */
		}

		/* 处理 Modbus 帧 (调用寄存器回调) */
		(void)eMBPoll();

		/* 检查是否有上位机命令需要处理 */
		if (HaveNewCMDFlag) {
			RS232CmdProcess();   /* 处理命令 (在 hostcmd.c 中) */
			HaveNewCMDFlag = false;
		}

		k_mutex_unlock(&ModbusBusMutex);  /* 释放总线 */
	}
}

/**
 * @brief USART3 Modbus 从站任务
 *
 * 管控仪专用 Modbus 从站, 固定地址 101, 波特率 115200。
 * 使用独立的 MB3 缓冲区, 不与 USART1/6 共享。
 *
 * 用途: 管控仪读取测量结果、校准参数、状态信息
 */
void task_modbus_usart3(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1); ARG_UNUSED(arg2); ARG_UNUSED(arg3);

	printk("Modbus USART3 task started\n");
	g_mb_inst = 3;  /* USART3 实例, 使用 MB3 独立缓冲区 */
	pMbInputData->nFirmVersion = FIRMWARE_VERSION;

	/* 初始化 FreeModbus: RTU 模式, 地址 101, USART3, 115200 baud */
	eMB3Init(MB_RTU, 101, 3, 115200, MB_PAR_NONE);
	eMB3Enable();

	while (1) {
		k_sem_take(&mb3_event_sem, K_MSEC(500));

		if (k_mutex_lock(&ModbusBusMutex, K_MSEC(200)) != 0) {
			continue;
		}

		(void)eMB3Poll();
		if (HaveNewCMDFlag) {
			RS232CmdProcess();
			HaveNewCMDFlag = false;
		}

		k_mutex_unlock(&ModbusBusMutex);
	}
}

/**
 * @brief USART6 Modbus 从站任务
 *
 * 扩展 Modbus 从站, 地址/波特率与 USART1 相同。
 * 共享主缓冲区, 与 USART1 互斥访问。
 *
 * 用途: 扩展通信 (预留, 可连接第二个上位机或 HMI)
 */
void task_modbus_usart6(void *arg1, void *arg2, void *arg3)
{
	ARG_UNUSED(arg1); ARG_UNUSED(arg2); ARG_UNUSED(arg3);

	printk("Modbus USART6 task started\n");
	g_mb_inst = 6;  /* USART6 实例, 共享主缓冲区 */
	pMbInputData->nFirmVersion = FIRMWARE_VERSION;

	/* 初始化 FreeModbus: RTU 模式, 地址 1, USART6, 115200 baud */
	eMB6Init(MB_RTU, 1, 6, 115200, MB_PAR_NONE);
	eMB6Enable();

	while (1) {
		k_sem_take(&mb6_event_sem, K_MSEC(500));

		if (k_mutex_lock(&ModbusBusMutex, K_MSEC(200)) != 0) {
			continue;
		}

		(void)eMB6Poll();
		if (HaveNewCMDFlag) {
			RS232CmdProcess();
			HaveNewCMDFlag = false;
		}

		k_mutex_unlock(&ModbusBusMutex);
	}
}
