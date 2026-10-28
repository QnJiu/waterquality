/*
 * mb_port_event.c — Zephyr k_sem 事件处理 (3 实例)
 *
 * 架构:
 *   xMBPortEventPost() 从 ISR (定时器/UART) 调用 → 存事件 + k_sem_give
 *   xMBPortEventGet()  从任务上下文调用 → 读事件 + 清标志
 *   信号量用于唤醒 Modbus 任务, 事件类型通过静态变量传递
 *
 * 注意: k_sem_give 是 ISR-safe, 可从定时器/UART ISR 安全调用
 */

#include "port.h"

/* ==================== 全局同步对象 ==================== */

/* 事件信号量 — ISR 中 give, Modbus 任务中 take */
struct k_sem mb1_event_sem;  /* USART1: 初始化 count=1 (首次 poll 不阻塞) */
struct k_sem mb3_event_sem;  /* USART3: 初始化 count=0 (等待首帧) */
struct k_sem mb6_event_sem;  /* USART6: 初始化 count=0 (等待首帧) */

/* 互斥锁 — 三路总线共享寄存器访问保护 */
struct k_mutex ModbusBusMutex;

/* 事件队列状态 — 每个实例独立的事件缓冲 */
static eMBEventType mb1_queued_event;   /* USART1 待处理事件类型 */
static bool        mb1_event_in_queue;  /* USART1 是否有未处理事件 */

static eMBEventType mb3_queued_event;   /* USART3 待处理事件类型 */
static bool        mb3_event_in_queue;  /* USART3 是否有未处理事件 */

static eMBEventType mb6_queued_event;   /* USART6 待处理事件类型 */
static bool        mb6_event_in_queue;  /* USART6 是否有未处理事件 */

/* ==================== Instance 1 (USART1) ==================== */

/**
 * @brief 初始化 USART1 事件 + 总线互斥锁
 * @note 信号量初始 count=1, 首次 k_sem_take 不阻塞 (触发首次 poll)
 * @note ModbusBusMutex 在此初始化, 三路共享
 */
BOOL xMBPortEventInit(void)
{
	k_sem_init(&mb1_event_sem, 1, 1);
	k_mutex_init(&ModbusBusMutex);
	mb1_event_in_queue = false;
	return TRUE;
}

/**
 * @brief 投递 USART1 事件 (ISR-safe)
 * @param eEvent 事件类型 (EV_READY/EV_FRAME_RECEIVED/EV_EXECUTE/EV_FRAME_SENT)
 * @note 可从 UART ISR 或定时器 ISR 调用 (k_sem_give 是 ISR-safe)
 */
BOOL xMBPortEventPost(eMBEventType eEvent)
{
	mb1_queued_event = eEvent;
	mb1_event_in_queue = true;
	k_sem_give(&mb1_event_sem);
	return TRUE;
}

/**
 * @brief 获取 USART1 待处理事件
 * @param eEvent 输出事件类型
 * @return TRUE=有事件, FALSE=无事件
 */
BOOL xMBPortEventGet(eMBEventType *eEvent)
{
	if (mb1_event_in_queue) {
		*eEvent = mb1_queued_event;
		mb1_event_in_queue = false;
		return TRUE;
	}
	return FALSE;
}

/* ==================== Instance 3 (USART3) ==================== */

/** @brief 初始化 USART3 事件信号量 (count=0, 等待首帧) */
BOOL xMBPortEventInit3(void)
{
	k_sem_init(&mb3_event_sem, 0, 1);
	mb3_event_in_queue = false;
	return TRUE;
}

/** @brief 投递 USART3 事件 (ISR-safe) */
BOOL xMBPortEventPost3(eMBEventType eEvent)
{
	mb3_queued_event = eEvent;
	mb3_event_in_queue = true;
	k_sem_give(&mb3_event_sem);
	return TRUE;
}

/** @brief 获取 USART3 待处理事件 */
BOOL xMBPortEventGet3(eMBEventType *eEvent)
{
	if (mb3_event_in_queue) {
		*eEvent = mb3_queued_event;
		mb3_event_in_queue = false;
		return TRUE;
	}
	return FALSE;
}

/* ==================== Instance 6 (USART6) ==================== */

/** @brief 初始化 USART6 事件信号量 (count=0, 等待首帧) */
BOOL xMBPortEventInit6(void)
{
	k_sem_init(&mb6_event_sem, 0, 1);
	mb6_event_in_queue = false;
	return TRUE;
}

/** @brief 投递 USART6 事件 (ISR-safe) */
BOOL xMBPortEventPost6(eMBEventType eEvent)
{
	mb6_queued_event = eEvent;
	mb6_event_in_queue = true;
	k_sem_give(&mb6_event_sem);
	return TRUE;
}

/** @brief 获取 USART6 待处理事件 */
BOOL xMBPortEventGet6(eMBEventType *eEvent)
{
	if (mb6_event_in_queue) {
		*eEvent = mb6_queued_event;
		mb6_event_in_queue = false;
		return TRUE;
	}
	return FALSE;
}
