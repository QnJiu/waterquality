#ifndef TP_F407_UART4_SUBORDINATE_H
#define TP_F407_UART4_SUBORDINATE_H
#include "tp_freertos_subordinate.h"
#include "task.h"

/* Reference only: UART4 PC10/PC11 AF8, 115200 8N1, full duplex.
 * Initialize once before the scheduler, and retain this object for its life. */
typedef struct { uint32_t us, ms; uint8_t byte; bool error; } tp_sub_uart_event;
typedef struct {
    tp_freertos_subordinate subordinate;
    QueueHandle_t events;
    TaskHandle_t task;
    uint8_t tx[TP_SUB_REQUEST_SIZE];
    volatile uint8_t tx_length, tx_position;
    volatile bool transmitting, overflow;
    void *wake_ctx;
    void (*set_wake)(void *, bool);
    uint32_t wake_delay_ms, wake_started_ms;
    bool wake_active, wake_waiting;
} tp_f407_uart4_subordinate;
tp_result tp_f407_uart4_subordinate_init(tp_f407_uart4_subordinate *,
    uint32_t period_ms,uint32_t timeout_ms,uint32_t freshness_ms,uint32_t first_token,
    uint16_t stack_words,UBaseType_t priority);
/* Optional active-high wake line, configured before scheduler start. The callback
 * must be short, nonblocking, and drive the level immediately; assertion
 * runs with interrupts masked.
 * The request timeout includes the wake delay. */
tp_result tp_f407_uart4_subordinate_set_wakeup(tp_f407_uart4_subordinate *,
    void *ctx, void (*set_wake)(void *, bool), uint32_t delay_ms);
#endif
