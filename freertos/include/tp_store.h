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
    uint8_t mode; /* controlled=0, periodic=1, daily=2 */
    uint16_t daily_minute;
    int16_t utc_offset_minutes;
} tp_config;
tp_result tp_config_validate(const tp_config *cfg);
/* A committed record using an unsupported version returns TP_UNSUPPORTED,
 * including when the other slot is readable; saves must not overwrite it. */
tp_result tp_store_load(const tp_store_io *io, tp_config *cfg, uint32_t *sequence);
tp_result tp_store_save(const tp_store_io *io, const tp_config *cfg);
/* A missing or corrupt pair uses supplied defaults in RAM only. I/O errors
 * and unsupported versions propagate without selecting defaults; no writes occur here. */
typedef enum { TP_CONFIG_FROM_STORAGE, TP_CONFIG_DEFAULTED } tp_config_source;
tp_result tp_store_bootstrap(const tp_store_io *io, const tp_config *defaults,
    tp_config *out, uint32_t *sequence, tp_config_source *source);
#endif
