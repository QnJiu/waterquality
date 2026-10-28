#include "tp_adc_reader.h"
#include <math.h>
tp_result tp_adc_reader_init(tp_adc_reader *r, tp_ads1220 *adc,
                             const tp_adc_channel *channels, size_t count) {
    if (!r || !adc || !adc->configured || !channels || !count || count > TP_ADC_MAX_CHANNELS)
        return TP_INVALID;
    for (size_t i = 0; i < count; i++)
        if (channels[i].mux > 14 || !isfinite(channels[i].volts_per_count) ||
            channels[i].volts_per_count <= 0) return TP_INVALID;
    r->adc = adc; r->channels = channels; r->count = count; r->active = false; return TP_OK;
}
tp_result tp_adc_reader_read(tp_adc_reader *r, uint8_t ch, uint32_t now, float *volts) {
    if (!r || !r->adc || !volts || ch >= r->count) return TP_INVALID;
    if (!r->active) {
        tp_result result = tp_ads1220_start(r->adc, r->channels[ch].mux, now);
        if (result != TP_OK) return result;
        r->active = true; r->active_channel = ch; return TP_BUSY;
    }
    if (ch != r->active_channel) return TP_BUSY;
    int32_t raw; tp_result result = tp_ads1220_poll(r->adc, now, &raw);
    if (result == TP_BUSY) return result;
    r->active = false;
    if (result != TP_OK) return result;
    if (raw == -8388608 || raw == 8388607) return TP_IO_ERROR;
    *volts = (float)raw * r->channels[ch].volts_per_count;
    return isfinite(*volts) ? TP_OK : TP_IO_ERROR;
}
tp_result tp_adc_reader_cancel(tp_adc_reader *r) {
    if (!r || !r->adc) return TP_INVALID;
    if (!r->active) return TP_OK;
    tp_result result = tp_ads1220_cancel(r->adc); r->active = false; return result;
}
