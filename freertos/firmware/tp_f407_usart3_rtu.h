#ifndef TP_F407_USART3_RTU_H
#define TP_F407_USART3_RTU_H
#include "tp_freertos_runtime.h"
/* Reference RS485: USART3 PD8 TX, PD9 RX, PD10 active-high DE (also tied to
 * /RE), 115200 8N1; TIM5 is reserved as a 1 MHz free-running clock.
 * One instance; init once before scheduler. Do not use from interrupt context.
 * IRQ event timestamps include interrupt latency; verify under system load. */
typedef struct { uint32_t at_us; uint8_t byte; bool error; } tp_uart_event;
typedef struct { uint16_t length; uint8_t data[TP_RUNTIME_FRAME_MAX]; } tp_uart_tx;
typedef struct {
    tp_freertos_runtime *runtime;
    tp_rtu_port port;
    QueueHandle_t events, responses;
    TaskHandle_t task;
    tp_uart_tx current;
    volatile uint16_t tx_pos;
    volatile bool transmitting, overflow;
    unsigned port_number;
} tp_f407_usart3_rtu;
tp_result tp_f407_usart3_rtu_init(tp_f407_usart3_rtu *uart,
    tp_freertos_runtime *runtime, unsigned port_number,
    uint16_t stack_words, UBaseType_t priority);
/* Copies one response. Queue-full and wrong port are reported to runtime. */
tp_result tp_f407_usart3_rtu_send(tp_f407_usart3_rtu *uart, unsigned port,
                                   const uint8_t *data, size_t length);
#endif
