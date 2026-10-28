#ifndef TP_MODBUS_H
#define TP_MODBUS_H
#include "tp_core.h"
/* One context per serial port. Call after a complete RTU frame has arrived;
 * the UART adapter handles 3.5-character framing and RS485 direction. */
typedef struct {
    void *ctx;
    uint8_t address;
    tp_result (*read_holding)(void *, uint16_t address, uint16_t count, uint16_t *values);
    /* Validate all values before modifying anything. Broadcast writes must also be atomic. */
    tp_result (*write_holding)(void *, uint16_t address, uint16_t count, const uint16_t *values);
} tp_modbus;
/* TP_BUSY means a valid broadcast was consumed and must not be answered. */
tp_result tp_modbus_frame(tp_modbus *m, const uint8_t *request, size_t request_size,
                          uint8_t *response, size_t capacity, size_t *response_size);
#endif
