/*
 * mb_port_serial.c — Zephyr UART 中断处理 (3 实例)
 *
 * RX 流程: ISR 读 byte → 存入 last_rx_byte → 回调 pxMBFrameCBByteReceived()
 *          → mbrtu.c 状态机调用 GetByte() → 返回 last_rx_byte
 * TX 流程: mbrtu.c 调用 PutByte() → uart_fifo_fill(单字节) → 硬件发送
 *          → TXE 中断触发 ISR → 回调 pxMBFrameCBTransmitterEmpty()
 *          → xMBRTUTransmitFSM() 调用 PutByte(下一个字节) → ...
 */

#include "port.h"
#include <zephyr/sys/printk.h>

/* ==================== UART 设备句柄 ==================== */
static const struct device *uart1_dev;  /* USART1 (PA9/PA10) — 主 Modbus */
static const struct device *uart3_dev;  /* USART3 (PD8/PD9) — 管控仪 Modbus */
static const struct device *uart6_dev;  /* USART6 (PC6/PC7) — 扩展 Modbus */

/* RX 缓冲: ISR 中 uart_fifo_read 读取的最后一个字节 */
static volatile CHAR uart1_last_rx_byte;
static volatile CHAR uart3_last_rx_byte;
static volatile CHAR uart6_last_rx_byte;

/* TX 状态: true=正在发送 (TX 中断使能中), false=发送完成或未开始 */
static volatile bool uart1_tx_active;
static volatile bool uart3_tx_active;
static volatile bool uart6_tx_active;

/* ==================== USART1 (PA9/PA10) — 主 Modbus ==================== */

/**
 * @brief USART1 中断服务例程
 *
 * RX 路径: uart_fifo_read → last_rx_byte → pxMBFrameCBByteReceived()
 *          mbrtu.c 状态机随后调用 GetByte() 取走字节
 * TX 路径: TXE 中断 → pxMBFrameCBTransmitterEmpty()
 *          mbrtu.c 状态机调用 PutByte() 发送下一字节
 */
static void uart1_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	/* RX: 读取所有可用字节, 逐个交给 FreeModbus RTU 接收状态机 */
	while (uart_irq_rx_ready(dev)) {
		uint8_t byte;
		uart_fifo_read(dev, &byte, 1);
		uart1_last_rx_byte = (CHAR)byte;
		if (pxMBFrameCBByteReceived) {
			pxMBFrameCBByteReceived();  /* 触发 mbrtu.c 接收状态机 */
		}
	}

	/* TX: 发送寄存器空 → 请求 FreeModbus 提供下一字节 */
	while (uart_irq_tx_ready(dev) && uart1_tx_active) {
		if (pxMBFrameCBTransmitterEmpty) {
			pxMBFrameCBTransmitterEmpty();  /* 触发 mbrtu.c 发送状态机 */
		}
	}
}

/**
 * @brief 初始化 USART1 串口 (中断模式)
 *
 * 获取设备句柄 → 注册 ISR → 禁用 TX。波特率在设备树中配置。
 * RX/TX 由 vMBPortSerialEnable() 动态控制。
 */
BOOL xMBPortSerialInit(UCHAR ucPort, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity)
{
	ARG_UNUSED(ucPort);
	ARG_UNUSED(ucDataBits);
	ARG_UNUSED(eParity);

	uart1_dev = DEVICE_DT_GET(DT_NODELABEL(usart1));
	if (!device_is_ready(uart1_dev)) {
		printk("USART1 not ready\n");
		return FALSE;
	}

	uart_irq_callback_user_data_set(uart1_dev, uart1_isr, NULL);
	uart1_tx_active = false;

	printk("MB1 serial init: USART1 %lu baud\n", ulBaudRate);
	return TRUE;
}

/**
 * @brief 使能/禁用 USART1 RX 和 TX 中断
 *
 * FreeModbus 核心在帧接收开始时使能 RX, 发送时使能 TX,
 * 发送完成后禁用 TX + 等待 TC。
 */
void vMBPortSerialEnable(BOOL xRxEnable, BOOL xTxEnable)
{
	if (xRxEnable) {
		uart_irq_rx_enable(uart1_dev);
	} else {
		uart_irq_rx_disable(uart1_dev);
	}
	if (xTxEnable) {
		uart1_tx_active = true;
		uart_irq_tx_enable(uart1_dev);
	} else {
		uart_irq_tx_disable(uart1_dev);
		uart1_tx_active = false;
	}
}

/** @brief 发送单字节 (mbrtu.c 发送状态机调用) */
BOOL xMBPortSerialPutByte(CHAR ucByte)
{
	uart_fifo_fill(uart1_dev, (const uint8_t *)&ucByte, 1);
	return TRUE;
}

/** @brief 读取 ISR 缓存的最后接收字节 (mbrtu.c 接收状态机调用) */
BOOL xMBPortSerialGetByte(CHAR *pucByte)
{
	*pucByte = uart1_last_rx_byte;
	return TRUE;
}

/**
 * @brief 等待 USART1 发送完成 (TC)
 *
 * busy-wait 等待 uart1_tx_active 变为 false。
 * 超时 2000000 次循环 (~200ms @ 168MHz)。
 */
BOOL xMBPortSerialWaitTC(void)
{
	int timeout = 2000000;
	while (uart1_tx_active && --timeout > 0) {
		/* busy-wait: TX ISR 禁用后 uart1_tx_active 变为 false */
	}
	return timeout > 0 ? TRUE : FALSE;
}

/* ==================== USART3 (PD8/PD9) — 管控仪 Modbus ==================== */

/** @brief USART3 ISR — 同 uart1_isr, 使用独立的回调函数指针 */
static void uart3_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_rx_ready(dev)) {
		uint8_t byte;
		uart_fifo_read(dev, &byte, 1);
		uart3_last_rx_byte = (CHAR)byte;
		if (pxMBFrameCBByteReceived3) {
			pxMBFrameCBByteReceived3();
		}
	}

	while (uart_irq_tx_ready(dev) && uart3_tx_active) {
		if (pxMBFrameCBTransmitterEmpty3) {
			pxMBFrameCBTransmitterEmpty3();
		}
	}
}

/** @brief 初始化 USART3 串口 (固定地址 101, 波特率 115200) */
BOOL xMBPortSerialInit3(UCHAR ucPort, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity)
{
	ARG_UNUSED(ucPort);
	ARG_UNUSED(ucDataBits);
	ARG_UNUSED(eParity);

	uart3_dev = DEVICE_DT_GET(DT_NODELABEL(usart3));
	if (!device_is_ready(uart3_dev)) {
		printk("USART3 not ready\n");
		return FALSE;
	}

	uart_irq_callback_user_data_set(uart3_dev, uart3_isr, NULL);
	uart3_tx_active = false;

	printk("MB3 serial init: USART3 %lu baud\n", ulBaudRate);
	return TRUE;
}

/** @brief 使能/禁用 USART3 RX 和 TX 中断 */
void vMBPortSerialEnable3(BOOL xRxEnable, BOOL xTxEnable)
{
	if (xRxEnable) {
		uart_irq_rx_enable(uart3_dev);
	} else {
		uart_irq_rx_disable(uart3_dev);
	}
	if (xTxEnable) {
		uart3_tx_active = true;
		uart_irq_tx_enable(uart3_dev);
	} else {
		uart_irq_tx_disable(uart3_dev);
		uart3_tx_active = false;
	}
}

/** @brief 发送单字节 (USART3) */
BOOL xMBPortSerialPutByte3(CHAR ucByte)
{
	uart_fifo_fill(uart3_dev, (const uint8_t *)&ucByte, 1);
	return TRUE;
}

/** @brief 读取 ISR 缓存的最后接收字节 (USART3) */
BOOL xMBPortSerialGetByte3(CHAR *pucByte)
{
	*pucByte = uart3_last_rx_byte;
	return TRUE;
}

/** @brief 等待 USART3 发送完成 */
BOOL xMBPortSerialWaitTC3(void)
{
	int timeout = 2000000;
	while (uart3_tx_active && --timeout > 0) {}
	return timeout > 0 ? TRUE : FALSE;
}

/* ==================== USART6 (PC6/PC7) — 扩展 Modbus ==================== */

/** @brief USART6 ISR — 同 uart1_isr, 使用独立的回调函数指针 */
static void uart6_isr(const struct device *dev, void *user_data)
{
	ARG_UNUSED(user_data);

	while (uart_irq_rx_ready(dev)) {
		uint8_t byte;
		uart_fifo_read(dev, &byte, 1);
		uart6_last_rx_byte = (CHAR)byte;
		if (pxMBFrameCBByteReceived6) {
			pxMBFrameCBByteReceived6();
		}
	}

	while (uart_irq_tx_ready(dev) && uart6_tx_active) {
		if (pxMBFrameCBTransmitterEmpty6) {
			pxMBFrameCBTransmitterEmpty6();
		}
	}
}

/** @brief 初始化 USART6 串口 (地址/波特率与 USART1 相同) */
BOOL xMBPortSerialInit6(UCHAR ucPort, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity)
{
	ARG_UNUSED(ucPort);
	ARG_UNUSED(ucDataBits);
	ARG_UNUSED(eParity);

	uart6_dev = DEVICE_DT_GET(DT_NODELABEL(usart6));
	if (!device_is_ready(uart6_dev)) {
		printk("USART6 not ready\n");
		return FALSE;
	}

	uart_irq_callback_user_data_set(uart6_dev, uart6_isr, NULL);
	uart6_tx_active = false;

	printk("MB6 serial init: USART6 %lu baud\n", ulBaudRate);
	return TRUE;
}

/** @brief 使能/禁用 USART6 RX 和 TX 中断 */
void vMBPortSerialEnable6(BOOL xRxEnable, BOOL xTxEnable)
{
	if (xRxEnable) {
		uart_irq_rx_enable(uart6_dev);
	} else {
		uart_irq_rx_disable(uart6_dev);
	}
	if (xTxEnable) {
		uart6_tx_active = true;
		uart_irq_tx_enable(uart6_dev);
	} else {
		uart_irq_tx_disable(uart6_dev);
		uart6_tx_active = false;
	}
}

/** @brief 发送单字节 (USART6) */
BOOL xMBPortSerialPutByte6(CHAR ucByte)
{
	uart_fifo_fill(uart6_dev, (const uint8_t *)&ucByte, 1);
	return TRUE;
}

/** @brief 读取 ISR 缓存的最后接收字节 (USART6) */
BOOL xMBPortSerialGetByte6(CHAR *pucByte)
{
	*pucByte = uart6_last_rx_byte;
	return TRUE;
}

/** @brief 等待 USART6 发送完成 */
BOOL xMBPortSerialWaitTC6(void)
{
	int timeout = 2000000;
	while (uart6_tx_active && --timeout > 0) {}
	return timeout > 0 ? TRUE : FALSE;
}
