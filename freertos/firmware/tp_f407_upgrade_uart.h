#ifndef TP_F407_UPGRADE_UART_H
#define TP_F407_UPGRADE_UART_H
#include "tp_upgrade_stream.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

/* Reference: USART2 PA2/PA3 AF7, APB1 16 MHz, 115200 8N1. The dedicated
 * upgrade task is the sole owner of commands and underlying boot/download
 * storage. Initialize before the scheduler; all objects must remain alive. */
typedef struct { uint32_t ms; uint8_t byte; bool error; } tp_upgrade_uart_event;
typedef struct {
    tp_upgrade_stream stream;
    QueueHandle_t events;
    TaskHandle_t task;
    volatile bool overflow;
    void *maintenance_ctx;
    tp_result (*request_maintenance)(void *);
    void *lock_ctx;
    bool (*try_lock)(void *);
    void (*unlock)(void *);
} tp_f407_upgrade_uart;
tp_result tp_f407_upgrade_uart_init(tp_f407_upgrade_uart *, const tp_upgrade_commands *,
                                    uint32_t frame_timeout_ms, uint16_t stack_words,
                                    UBaseType_t priority);
/* The shared lock also protects trial confirmation and all image modifications.
 * Lock acquisition is nonblocking; a busy request gets TP_LOCK_BUSY and can be
 * retried. The lock is released before the response is transmitted. */
tp_result tp_f407_upgrade_uart_init_shared(tp_f407_upgrade_uart *, const tp_upgrade_commands *,
    uint32_t frame_timeout_ms, uint16_t stack_words, UBaseType_t priority,
    void *lock_ctx, bool (*try_lock)(void *), void (*unlock)(void *));
/* Mutating commands wait for the nonblocking maintenance callback to return
 * TP_OK. TP_LOCK_BUSY means no command was executed and can be retried. */
tp_result tp_f407_upgrade_uart_init_guarded(tp_f407_upgrade_uart *, const tp_upgrade_commands *,
    uint32_t frame_timeout_ms, uint16_t stack_words, UBaseType_t priority,
    void *lock_ctx, bool (*try_lock)(void *), void (*unlock)(void *),
    void *maintenance_ctx, tp_result (*request_maintenance)(void *));
#endif
