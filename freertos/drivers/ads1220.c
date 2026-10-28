#include "tp_ads1220.h"
#include <string.h>

static tp_result command(tp_ads1220 *a, uint8_t cmd) {
    uint8_t rx; return a->io.transfer(a->io.ctx, &cmd, &rx, 1);
}
tp_result tp_ads1220_init(tp_ads1220 *a, const tp_ads1220_io *io,
                          const uint8_t regs[4], uint32_t timeout_ms) {
    if (!a || !io || !io->transfer || !io->data_ready || !regs || !timeout_ms ||
        timeout_ms > 0x7fffffffu || (regs[1] & 0x06u) || (regs[0] >> 4) > 14u)
        return TP_INVALID;
    /* DRDYM must be disabled when using the dedicated DRDY pin. */
    if (regs[3] & 0x02u) return TP_INVALID;
    memset(a, 0, sizeof(*a)); a->io = *io; a->timeout_ms = timeout_ms;
    uint8_t tx[5] = { 0x43u, regs[0], regs[1], regs[2], regs[3] }, rx[5];
    if (a->io.transfer(a->io.ctx, tx, rx, sizeof(tx)) != TP_OK) return TP_IO_ERROR;
    uint8_t rd[5] = { 0x23u, 0xffu, 0xffu, 0xffu, 0xffu };
    if (a->io.transfer(a->io.ctx, rd, rx, sizeof(rd)) != TP_OK ||
        memcmp(rx + 1, regs, 4) != 0) return TP_IO_ERROR;
    memcpy(a->registers, regs, 4); a->configured = true; a->baseline_valid = true; return TP_OK;
}
tp_result tp_ads1220_recover(tp_ads1220 *a) {
    if (!a || !a->io.transfer || !a->io.data_ready || !a->timeout_ms || !a->baseline_valid) return TP_INVALID;
    if (a->pending) return TP_BUSY;
    /* Keep the last verified settings even if an attempted recovery fails. */
    tp_ads1220_io io = a->io;
    uint8_t regs[4]; memcpy(regs, a->registers, sizeof(regs));
    uint32_t timeout = a->timeout_ms;
    a->configured = false;
    if (command(a, 0x02u) != TP_OK) return TP_IO_ERROR;
    tp_ads1220 candidate;
    tp_result result = tp_ads1220_init(&candidate, &io, regs, timeout);
    if (result == TP_OK) *a = candidate;
    return result;
}
tp_result tp_ads1220_start(tp_ads1220 *a, uint8_t mux, uint32_t now) {
    if (!a || !a->configured || mux > 14u) return TP_INVALID;
    if (a->pending) return TP_BUSY;
    uint8_t reg = (uint8_t)((a->registers[0] & 0x0fu) | (mux << 4));
    uint8_t tx[2] = { 0x40u, reg }, rx[2];
    if (a->io.transfer(a->io.ctx, tx, rx, sizeof(tx)) != TP_OK) {
        a->configured = false; return TP_IO_ERROR;
    }
    a->registers[0] = reg;
    if (command(a, 0x08u) != TP_OK) { a->configured = false; return TP_IO_ERROR; }
    a->started_ms = now; a->pending = true; return TP_OK;
}
tp_result tp_ads1220_poll(tp_ads1220 *a, uint32_t now, int32_t *raw) {
    if (!a || !a->configured || !a->pending || !raw) return TP_INVALID;
    if ((uint32_t)(now - a->started_ms) >= a->timeout_ms) {
        (void)command(a, 0x02u); a->pending = false; a->configured = false; return TP_TIMEOUT;
    }
    bool ready = false;
    if (a->io.data_ready(a->io.ctx, &ready) != TP_OK) {
        a->pending = false; a->configured = false; return TP_IO_ERROR;
    }
    if (!ready) return TP_BUSY;
    uint8_t tx[4] = { 0x10u, 0xffu, 0xffu, 0xffu }, rx[4];
    if (a->io.transfer(a->io.ctx, tx, rx, sizeof(tx)) != TP_OK) {
        a->pending = false; a->configured = false; return TP_IO_ERROR;
    }
    uint32_t bits = ((uint32_t)rx[1] << 16) | ((uint32_t)rx[2] << 8) | rx[3];
    *raw = (bits & 0x800000u) ? (int32_t)(bits & 0x7fffffu) - 0x800000 : (int32_t)bits;
    a->pending = false; return TP_OK;
}
tp_result tp_ads1220_cancel(tp_ads1220 *a) {
    if (!a || !a->configured) return TP_INVALID;
    tp_result r = command(a, 0x02u); a->pending = false;
    if (r != TP_OK) { a->configured = false; return TP_IO_ERROR; }
    return TP_OK;
}
