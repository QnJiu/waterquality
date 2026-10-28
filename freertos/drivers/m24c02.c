#include "tp_m24c02.h"
#include <string.h>
#define MEMORY_SIZE 256u
#define PAGE_SIZE 8u

tp_result tp_m24c02_init(tp_m24c02 *d, const tp_i2c_io *bus, uint8_t address,
                         void *ctx, tp_result (*wait_ready)(void *, uint8_t)) {
    if (!d || !bus || !bus->write || !bus->write_read || !wait_ready ||
        address < 0x50u || address > 0x57u) return TP_INVALID;
    d->bus = *bus; d->address = address; d->wait_ctx = ctx; d->wait_ready = wait_ready;
    return TP_OK;
}
static bool valid(tp_m24c02 *d, uint16_t offset, const void *data, size_t size) {
    return d && d->bus.write && d->bus.write_read && d->wait_ready &&
        d->address >= 0x50u && d->address <= 0x57u && offset <= MEMORY_SIZE &&
        size <= MEMORY_SIZE - offset && (data || !size);
}
tp_result tp_m24c02_read(tp_m24c02 *d, uint16_t offset, void *data, size_t size) {
    if (!valid(d, offset, data, size)) return TP_INVALID;
    if (!size) return TP_OK;
    uint8_t addr = (uint8_t)offset;
    return d->bus.write_read(d->bus.ctx, d->address, &addr, 1, data, size) == TP_OK
        ? TP_OK : TP_IO_ERROR;
}
tp_result tp_m24c02_write(tp_m24c02 *d, uint16_t offset, const void *data, size_t size) {
    if (!valid(d, offset, data, size)) return TP_INVALID;
    const uint8_t *p = data;
    while (size) {
        size_t n = PAGE_SIZE - (offset % PAGE_SIZE);
        if (n > size) n = size;
        uint8_t tx[PAGE_SIZE + 1]; tx[0] = (uint8_t)offset;
        memcpy(tx + 1, p, n);
        if (d->bus.write(d->bus.ctx, d->address, tx, n + 1) != TP_OK) return TP_IO_ERROR;
        if (d->wait_ready(d->wait_ctx, d->address) != TP_OK) return TP_IO_ERROR;
        offset = (uint16_t)(offset + n); p += n; size -= n;
    }
    return TP_OK;
}
