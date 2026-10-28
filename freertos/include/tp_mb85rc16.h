#ifndef TP_MB85RC16_H
#define TP_MB85RC16_H
#include "tp_store.h"
/* MB85RC16 uses 7-bit addresses 0x50..0x57; upper memory address bits are
 * encoded in the device address. Transfers must finish before returning. */
typedef struct {
    void *ctx;
    tp_result (*write)(void *ctx, uint8_t address7, const uint8_t *data, size_t size);
    tp_result (*write_read)(void *ctx, uint8_t address7, const uint8_t *tx,
                            size_t tx_size, uint8_t *rx, size_t rx_size);
} tp_i2c_io;
typedef struct { tp_i2c_io io; uint16_t base; } tp_mb85rc16;
/* base is 256-byte aligned and reserves 2 independent 128-byte config slots. */
tp_result tp_mb85rc16_init(tp_mb85rc16 *dev, const tp_i2c_io *io, uint16_t base);
tp_store_io tp_mb85rc16_store(tp_mb85rc16 *dev);
#endif
