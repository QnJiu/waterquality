#include "tp_ads1220.h"
#include "tp_schedule.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct { uint8_t regs[4]; bool ready; uint32_t raw; int commands; } fake_adc;
static tp_result transfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t n) {
    fake_adc *f = ctx; memset(rx, 0, n); f->commands++;
    if (n == 5 && tx[0] == 0x43) memcpy(f->regs, tx + 1, 4);
    else if (n == 5 && tx[0] == 0x23) memcpy(rx + 1, f->regs, 4);
    else if (n == 2 && tx[0] == 0x40) f->regs[0] = tx[1];
    else if (n == 4 && tx[0] == 0x10) {
        rx[1] = (uint8_t)(f->raw >> 16); rx[2] = (uint8_t)(f->raw >> 8); rx[3] = (uint8_t)f->raw;
    } else if (n != 1 || (tx[0] != 0x08 && tx[0] != 0x02)) return TP_IO_ERROR;
    return TP_OK;
}
static tp_result ready(void *ctx, bool *r) { *r = ((fake_adc *)ctx)->ready; return TP_OK; }
int main(void) {
    fake_adc f = {0}; tp_ads1220 a; tp_ads1220_io io = {&f, transfer, ready};
    uint8_t regs[4] = {0x01, 0, 0, 0}; int32_t raw = 0;
    assert(tp_ads1220_init(&a, &io, regs, 100) == TP_OK);
    assert(tp_ads1220_start(&a, 11, 0xfffffff0u) == TP_OK && f.regs[0] == 0xb1);
    assert(tp_ads1220_start(&a, 10, 0) == TP_BUSY);
    assert(tp_ads1220_poll(&a, 0, &raw) == TP_BUSY);
    f.ready = true; f.raw = 0x800000;
    assert(tp_ads1220_poll(&a, 1, &raw) == TP_OK && raw == -8388608);
    assert(tp_ads1220_start(&a, 8, 10) == TP_OK);
    f.ready = false; assert(tp_ads1220_poll(&a, 110, &raw) == TP_TIMEOUT && !a.configured);
    tp_schedule s;
    assert(tp_schedule_configure(&s, TP_MODE_PERIODIC, 100, 0, 0, 0xfffffff0u, false) == TP_OK);
    assert(!tp_schedule_poll(&s, 20, false, 0, false));
    assert(tp_schedule_poll(&s, 84, false, 0, false));
    assert(!tp_schedule_poll(&s, 184, false, 0, true));
    assert(!tp_schedule_poll(&s, 185, false, 0, false));
    assert(tp_schedule_configure(&s, TP_MODE_DAILY, 0, 60, 0, 0, false) == TP_OK);
    assert(!tp_schedule_poll(&s, 0, false, 3600, false));
    assert(tp_schedule_poll(&s, 0, true, 3600, false));
    assert(!tp_schedule_poll(&s, 0, true, 3630, false));
    assert(!tp_schedule_poll(&s, 0, true, 3600, false));
    assert(tp_schedule_poll(&s, 0, true, 90000, false));
    puts("ADS1220 and schedule checks passed"); return 0;
}
