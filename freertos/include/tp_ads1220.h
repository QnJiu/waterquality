#ifndef TP_ADS1220_H
#define TP_ADS1220_H
#include "tp_core.h"
/* One transfer is one complete CS-low transaction. Adapter must serialize SPI
 * access, keep CS asserted across all bytes, and return it high on every exit. */
typedef struct {
    void *ctx;
    tp_result (*transfer)(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len);
    tp_result (*data_ready)(void *ctx, bool *ready); /* dedicated DRDY is active low */
} tp_ads1220_io;
typedef struct {
    tp_ads1220_io io;
    uint8_t registers[4];
    uint32_t started_ms, timeout_ms;
    bool configured, pending, baseline_valid;
} tp_ads1220;
/* Configure only while idle. reg1 must select single-shot conversion and external
 * inputs; raw register values are provided by the board configuration. */
tp_result tp_ads1220_init(tp_ads1220 *adc, const tp_ads1220_io *io,
                          const uint8_t registers[4], uint32_t timeout_ms);
/* Reconfigure a previously initialized, idle device after an I/O error. Sends
 * POWERDOWN, then writes and verifies the last known configuration. This does
 * not test a conversion or repair wiring. Calls must be serialized with reads. */
tp_result tp_ads1220_recover(tp_ads1220 *adc);
/* mux is the unshifted 4-bit MUX code (0..14). */
tp_result tp_ads1220_start(tp_ads1220 *adc, uint8_t mux, uint32_t now_ms);
tp_result tp_ads1220_poll(tp_ads1220 *adc, uint32_t now_ms, int32_t *raw);
tp_result tp_ads1220_cancel(tp_ads1220 *adc);
#endif
