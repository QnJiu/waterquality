#ifndef TP_F407_I2C_H
#define TP_F407_I2C_H
#include "tp_mb85rc16.h"
#include "FreeRTOS.h"
#include "semphr.h"
/* Reference bus: I2C2 PB10/PB11 AF4, external pull-ups, 100 kHz, APB1 16 MHz.
 * One instance owns I2C2. Init once before scheduler; context remains valid.
 * Transactions are blocking with bounded timeout and must run in a low-priority
 * storage task, never in an ISR or the control task. */
typedef struct { SemaphoreHandle_t mutex; } tp_f407_i2c;
tp_result tp_f407_i2c_init(tp_f407_i2c *bus);
tp_i2c_io tp_f407_i2c_io(tp_f407_i2c *bus);
#endif
