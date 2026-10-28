#ifndef TP_F407_TEMPERATURE_H
#define TP_F407_TEMPERATURE_H
#include "tp_ntc.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
/* ADC1 channel 0 PA0, 10k pull-up to ADC reference and 10k B3950 NTC to GND.
 * Owns ADC1/common ADC prescaler; initialization resets all ADCs. Dedicated low-priority polling task. */
typedef struct { float celsius; uint32_t at_ms; bool valid; } tp_temperature_sample;
typedef struct { QueueHandle_t samples; TaskHandle_t task; uint32_t max_age_ms; } tp_f407_temperature;
tp_result tp_f407_temperature_start(tp_f407_temperature *sensor,uint32_t max_age_ms,
    uint16_t stack_words,UBaseType_t priority);
/* Nonblocking; fails closed when no valid fresh sample exists. */
tp_result tp_f407_temperature_read(tp_f407_temperature *sensor,float *celsius);
#endif
