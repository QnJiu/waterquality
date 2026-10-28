#include "tp_adc_status.h"
#include <math.h>
#include <string.h>

tp_result tp_adc_status_init(tp_adc_status *s, tp_adc_reader *reader,
                             uint8_t channel, uint32_t period, uint32_t now) {
    if (!s || !reader || !reader->adc || channel >= reader->count ||
        !period || period > 0x7fffffffu) return TP_INVALID;
    memset(s, 0, sizeof(*s));
    s->reader = reader; s->channel = channel; s->period_ms = period; s->next_ms = now;
    return TP_OK;
}
void tp_adc_status_poll(tp_adc_status *s, uint32_t now) {
    if (!s || !s->reader) return;
    if (!s->sampling && (int32_t)(now - s->next_ms) < 0) return;
    float volts = 0;
    tp_result r = tp_adc_reader_read(s->reader, s->channel, now, &volts);
    if (r == TP_BUSY) { s->sampling = true; return; }
    s->sampling = false; s->next_ms = now + s->period_ms;
    if (r == TP_OK) {
        double uv = round((double)volts * 1000000.0);
        if (!isfinite(uv) || uv < -2147483648.0 || uv > 2147483647.0) r = TP_IO_ERROR;
        else { s->microvolts = (int32_t)uv; s->sequence++; }
    }
    s->result = (uint16_t)r;
    s->status = r == TP_OK ? 1u : 2u;
    if (r != TP_OK) s->microvolts = 0;
    s->revision++;
}
tp_result tp_adc_status_read(void *ctx, uint16_t address, uint16_t count, uint16_t *values) {
    tp_adc_status *s = ctx;
    if (!s || !values || !count || (uint32_t)address + count > 14u) return TP_INVALID;
    uint32_t uv = (uint32_t)s->microvolts;
    uint16_t regs[14] = {s->status, s->result, s->channel, s->sequence,
                         (uint16_t)(uv >> 16), (uint16_t)uv,
                         s->revision, s->status, s->result, s->channel, s->sequence,
                         (uint16_t)(uv >> 16), (uint16_t)uv, s->revision};
    memcpy(values, regs + address, count * sizeof(*values));
    return TP_OK;
}
