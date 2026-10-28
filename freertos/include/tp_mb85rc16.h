#ifndef TP_MB85RC16_H
#define TP_MB85RC16_H
#include "tp_store.h"
#include "tp_event_log.h"
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
/* Log region must be 32-byte aligned, within the device, and disjoint from
 * the two config slots. Region and device contexts must remain alive. */
typedef struct { tp_mb85rc16 *dev; uint16_t start, size; } tp_mb85rc16_log_region;
tp_result tp_mb85rc16_log_init(tp_mb85rc16_log_region *region, tp_mb85rc16 *dev,
                               uint16_t start, uint16_t size);
tp_event_log_io tp_mb85rc16_log_io(tp_mb85rc16_log_region *region);
#endif
