#ifndef TP_ADC_STATUS_H
#define TP_ADC_STATUS_H
#include "tp_adc_reader.h"
#include "tp_modbus.h"

/* Single-owner sampling and register service. Register addresses 0..5:
 * status (0 waiting, 1 valid, 2 error), tp_result, channel, sample sequence
 * (16-bit wrapping), signed microvolts high word then low word, rounded to the
 * nearest microvolt (half away from zero). On failure the
 * value is cleared. External synchronization is required for multiple tasks. */
typedef struct {
    tp_adc_reader *reader;
    uint32_t period_ms, next_ms;
    uint8_t channel;
    bool sampling;
    uint16_t status, result, sequence;
    int32_t microvolts;
} tp_adc_status;
tp_result tp_adc_status_init(tp_adc_status *s, tp_adc_reader *reader,
                             uint8_t channel, uint32_t period_ms, uint32_t now_ms);
void tp_adc_status_poll(tp_adc_status *s, uint32_t now_ms);
tp_result tp_adc_status_read(void *ctx, uint16_t address, uint16_t count, uint16_t *values);
#endif
