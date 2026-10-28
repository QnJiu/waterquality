#ifndef TP_ADC_READER_H
#define TP_ADC_READER_H
#include "tp_ads1220.h"
#define TP_ADC_MAX_CHANNELS 8u
typedef struct {
    uint8_t mux;              /* ADS1220 MUX code */
    float volts_per_count;    /* Includes reference and PGA gain */
} tp_adc_channel;
typedef struct {
    tp_ads1220 *adc;
    const tp_adc_channel *channels;
    size_t count;
    uint8_t active_channel;
    bool active;
} tp_adc_reader;
tp_result tp_adc_reader_init(tp_adc_reader *r, tp_ads1220 *adc,
                             const tp_adc_channel *channels, size_t count);
/* Invoke from one task; repeat until TP_OK or error. Caller supplies monotonic time.
 * ADC endpoints are reported as sensor faults rather than accepted measurements. */
tp_result tp_adc_reader_read(tp_adc_reader *r, uint8_t channel, uint32_t now_ms, float *volts);
tp_result tp_adc_reader_cancel(tp_adc_reader *r);
#endif
