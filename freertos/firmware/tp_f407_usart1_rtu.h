#ifndef TP_F407_USART1_RTU_H
#define TP_F407_USART1_RTU_H
#include "tp_f407_usart3_rtu.h"
/* Optional full-duplex USART1: PA9 TX, PA10 RX, 115200 8N1.
 * Use an RS232 level converter; never connect MCU pins directly to RS232.
 * TIM5 supplies timestamps. One instance, initialized before the scheduler. */
typedef tp_f407_usart3_rtu tp_f407_usart1_rtu;
tp_result tp_f407_usart1_rtu_init(tp_f407_usart1_rtu *uart,
    tp_freertos_runtime *runtime, unsigned port_number,
    uint16_t stack_words, UBaseType_t priority);
tp_result tp_f407_usart1_rtu_send(tp_f407_usart1_rtu *uart, unsigned port,
    const uint8_t *data, size_t length);
#endif
