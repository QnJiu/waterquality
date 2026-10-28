#ifndef TP_STORE_H
#define TP_STORE_H
#include "tp_core.h"
/* Two independently writable slots; callback writes must complete or return an error.
 * Slot size >= 128 bytes. Erasing one slot must not affect the other. */
typedef struct {
    void *ctx;
    tp_result (*read)(void *ctx, unsigned slot, uint32_t offset, void *data, size_t size);
    tp_result (*erase)(void *ctx, unsigned slot);
    tp_result (*write)(void *ctx, unsigned slot, uint32_t offset, const void *data, size_t size);
} tp_store_io;
typedef struct {
    tp_calibration calibration;
    float kp, ki, kd, deadband_c, output_max, integral_max, max_temperature_c;
    uint32_t period_seconds;
    uint8_t slave_address;
} tp_config;
tp_result tp_config_validate(const tp_config *cfg);
tp_result tp_store_load(const tp_store_io *io, tp_config *cfg, uint32_t *sequence);
tp_result tp_store_save(const tp_store_io *io, const tp_config *cfg);
#endif
