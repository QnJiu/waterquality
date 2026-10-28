#ifndef TP_FREEMODBUS_FRAME_H
#define TP_FREEMODBUS_FRAME_H
#include "tp_modbus.h"
/* Single owner across all runtime instances: upstream FreeModbus is global.
 * This function is not thread-safe; current is not a synchronization lock.
 * The UART task must first complete RTU timing/error framing. */
tp_result tp_freemodbus_frame(tp_modbus *m, const uint8_t *request, size_t length,
                              uint8_t *response, size_t capacity, size_t *out);
#endif
