/*
 * mb_port.h — Zephyr FreeModbus 公共头文件
 *
 * 包含:
 *   1. 临界区宏 (ENTER/EXIT_CRITICAL_SECTION) — 使用 CMSIS __disable_irq
 *   2. 三路 Modbus 事件信号量 (mb1/3/6_event_sem) — ISR 中 give, 任务中 take
 *   3. 总线互斥锁 (ModbusBusMutex) — 三路共享寄存器访问保护
 *
 * FreeModbus 类型定义在 port.h (BOOL/UCHAR 等) 和 mbport.h (eMBErrorCode 等)
 */

#ifndef MB_PORT_ZEPHYR_H
#define MB_PORT_ZEPHYR_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 临界区保护 — 使用 CMSIS 全局中断禁用
 *
 * FreeModbus 核心在寄存器读写时使用 EnterCriticalSection/ExitCriticalSection
 * 保护共享数据。由于 FreeModbus 不嵌套临界区, 简单使用 __disable_irq 即可。
 *
 * 注意: 这会禁用所有中断, 临界区内代码必须快速执行。
 */
#include <cmsis_core.h>
#define ENTER_CRITICAL_SECTION()  __disable_irq()
#define EXIT_CRITICAL_SECTION()   __enable_irq()
#define EnterCriticalSection()    __disable_irq()
#define ExitCriticalSection()     __enable_irq()

/*
 * 三路 Modbus 事件信号量 — 用于 ISR → 任务唤醒
 *
 * UART ISR 接收完一帧后 give 信号量, 对应 Modbus 任务 take 后调用 eMBPoll()。
 * 500ms 超时保证即使无通信也能周期唤醒。
 */
extern struct k_sem mb1_event_sem;  /* USART1 事件信号量 */
extern struct k_sem mb3_event_sem;  /* USART3 事件信号量 */
extern struct k_sem mb6_event_sem;  /* USART6 事件信号量 */

/*
 * 总线互斥锁 — 三路 Modbus 共享寄存器保护
 *
 * 三路 Modbus 任务共享 pMbHoldData/pMbInputData 寄存器缓冲区,
 * eMBPoll() 和 RS232CmdProcess() 执行时需持锁 (200ms 超时)。
 */
extern struct k_mutex ModbusBusMutex;

#ifdef __cplusplus
}
#endif

#endif /* MB_PORT_ZEPHYR_H */
