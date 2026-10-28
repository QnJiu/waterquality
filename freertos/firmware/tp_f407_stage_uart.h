#ifndef TP_F407_STAGE_UART_H
#define TP_F407_STAGE_UART_H
#include "tp_freertos_stage_transport.h"
#include "queue.h"
/* USART6 PC6/PC7, 16 MHz APB2, 115200 8N1. One task owns callbacks. */
typedef struct {
    QueueHandle_t events;
    volatile bool overflow;
    bool started;
    void *finished_ctx;
    void (*finished)(void *,tp_result);
} tp_f407_stage_uart;
/* Initialize once before scheduler; object and callback context stay alive. */
tp_result tp_f407_stage_uart_init(tp_f407_stage_uart *,tp_stage_transport_io *,
                                  void *,void (*finished)(void *,tp_result));
#endif
