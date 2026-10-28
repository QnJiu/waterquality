#include "tp_adc_status.h"
#include "adc_slice_vector.h"
#include <assert.h>
#include <string.h>

typedef struct { uint8_t regs[4]; bool ready; uint32_t raw; } fake_adc;
static tp_result transfer(void *ctx, const uint8_t *tx, uint8_t *rx, size_t n) {
    fake_adc *f = ctx; memset(rx, 0, n);
    if (n == 5 && tx[0] == 0x43) memcpy(f->regs, tx + 1, 4);
    else if (n == 5 && tx[0] == 0x23) memcpy(rx + 1, f->regs, 4);
    else if (n == 2 && tx[0] == 0x40) f->regs[0] = tx[1];
    else if (n == 4 && tx[0] == 0x10) {
        rx[1] = (uint8_t)(f->raw >> 16); rx[2] = (uint8_t)(f->raw >> 8); rx[3] = (uint8_t)f->raw;
    } else if (n != 1 || (tx[0] != 0x08 && tx[0] != 0x02)) return TP_IO_ERROR;
    return TP_OK;
}
static tp_result ready(void *ctx, bool *r) { *r = ((fake_adc *)ctx)->ready; return TP_OK; }
static void shared_slice_vector(void) {
    fake_adc f = {.ready = true, .raw = TP_SLICE_RAW};
    tp_ads1220 a; tp_ads1220_io io = {&f, transfer, ready};
    uint8_t regs[4] = {1,0,0,0};
    tp_adc_channel ch = {8, TP_SLICE_VOLTS_PER_COUNT};
    tp_adc_reader reader; tp_adc_status status;
    assert(tp_ads1220_init(&a, &io, regs, 100) == TP_OK);
    assert(tp_adc_reader_init(&reader, &a, &ch, 1) == TP_OK);
    assert(tp_adc_status_init(&status, &reader, 0, 1000, 0) == TP_OK);
    tp_adc_status_poll(&status, 0);
    tp_adc_status_poll(&status, 1);
    assert(status.status == 1 && status.microvolts == TP_SLICE_UV);
    tp_modbus modbus = {&status, 1, tp_adc_status_read, NULL};
    const uint8_t request[] = TP_SLICE_REQUEST;
    uint8_t reply[32]; size_t length = 0;
    assert(tp_modbus_frame(&modbus, request, sizeof(request), reply, sizeof(reply), &length) == TP_OK);
    assert(length == 17 && reply[0] == 1 && reply[1] == 3 && reply[2] == 12);
    assert(reply[3] == 0 && reply[4] == 1 && reply[5] == 0 && reply[6] == 0);
    assert(reply[7] == 0 && reply[8] == 0 && reply[9] == 0 && reply[10] == 1);
    assert(reply[11] == (uint8_t)(TP_SLICE_UV >> 24) && reply[12] == (uint8_t)(TP_SLICE_UV >> 16));
    assert(reply[13] == (uint8_t)(TP_SLICE_UV >> 8) && reply[14] == (uint8_t)TP_SLICE_UV);
}
int main(void) {
    shared_slice_vector();
    fake_adc f = {0}; tp_ads1220 a; tp_ads1220_io io = {&f, transfer, ready};
    uint8_t regs[4] = {1,0,0,0}; tp_adc_channel ch = {8, 0.000001f};
    tp_adc_reader reader; tp_adc_status s; uint16_t values[6];
    assert(tp_ads1220_init(&a, &io, regs, 50) == TP_OK);
    assert(tp_adc_reader_init(&reader, &a, &ch, 1) == TP_OK);
    assert(tp_adc_reader_ready(&reader) && !tp_adc_reader_ready(NULL));
    assert(tp_adc_status_init(&s, &reader, 0, 100, 0xfffffff0u) == TP_OK);
    tp_adc_status_poll(&s, 0xfffffff0u); assert(s.sampling && !s.status);
    assert(!tp_adc_reader_ready(&reader));
    f.ready = true; f.raw = 1234; tp_adc_status_poll(&s, 0xfffffff1u);
    assert(s.status == 1 && s.sequence == 1 && s.microvolts == 1234);
    assert(tp_adc_reader_ready(&reader));
    assert(tp_adc_status_read(&s, 0, 6, values) == TP_OK);
    assert(values[0] == 1 && values[3] == 1 && values[4] == 0 && values[5] == 1234);
    tp_modbus modbus = {&s, 1, tp_adc_status_read, NULL};
    const uint8_t request[] = {1, 3, 0, 0, 0, 6, 0xc5, 0xc8};
    uint8_t reply[32]; size_t length = 0;
    assert(tp_modbus_frame(&modbus, request, sizeof(request), reply, sizeof(reply), &length) == TP_OK);
    assert(length == 17 && reply[0] == 1 && reply[1] == 3 && reply[2] == 12);
    assert(reply[4] == 1 && reply[10] == 1 && reply[13] == 4 && reply[14] == 0xd2);
    assert(tp_adc_status_read(&s, 5, 2, values) == TP_INVALID);
    tp_adc_status_poll(&s, 84); assert(!s.sampling);
    tp_adc_status_poll(&s, 85); assert(s.sampling);
    f.ready = false; tp_adc_status_poll(&s, 135);
    assert(s.status == 2 && s.result == TP_TIMEOUT && s.microvolts == 0 && s.sequence == 1);
    assert(!tp_adc_reader_ready(&reader)); /* Clear-fault alone cannot restore the ADC. */
    return 0;
}
