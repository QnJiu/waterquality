#ifndef TP_M24C02_H
#define TP_M24C02_H
#include "tp_mb85rc16.h" /* shared synchronous I2C interface */
/* M24C02: 256 bytes, writes split on 8-byte boundaries, address 0x50..0x57.
 * wait_ready must wait until the internal write cycle finishes (or return an
 * error on timeout). It may sleep in a storage task; do not call from an ISR.
 * The caller serializes access to this device, including the write cycle. After
 * a write or wait error the device may still be busy; recover before reuse. */
typedef struct {
    tp_i2c_io bus;
    uint8_t address;
    void *wait_ctx;
    tp_result (*wait_ready)(void *ctx, uint8_t address7);
} tp_m24c02;
tp_result tp_m24c02_init(tp_m24c02 *dev, const tp_i2c_io *bus, uint8_t address7,
                         void *wait_ctx, tp_result (*wait_ready)(void *, uint8_t));
tp_result tp_m24c02_read(tp_m24c02 *dev, uint16_t offset, void *data, size_t size);
/* Each page is committed separately. On error, earlier pages may have changed. */
tp_result tp_m24c02_write(tp_m24c02 *dev, uint16_t offset, const void *data, size_t size);
#endif
