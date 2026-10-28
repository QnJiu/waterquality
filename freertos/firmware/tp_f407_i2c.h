#ifndef TP_F407_I2C_H
#define TP_F407_I2C_H
#include "tp_mb85rc16.h"
#ifndef TP_F407_I2C_POLL_ONLY
#include "FreeRTOS.h"
#include "semphr.h"
#endif
/* Reference buses: I2C2 PB10/PB11 or I2C3 PA8/PC9, AF4, external
 * pull-ups, 100 kHz, APB1 16 MHz. One instance owns each peripheral. Init once before scheduler; context remains valid.
 * Transactions are blocking with bounded timeout and must run in a low-priority
 * storage task, never in an ISR or the control task. */
#ifdef TP_F407_I2C_POLL_ONLY
/* Single owner, no interrupts may access this peripheral. */
typedef struct { bool initialized; unsigned peripheral; } tp_f407_i2c;
#else
typedef struct { SemaphoreHandle_t mutex; unsigned peripheral; } tp_f407_i2c;
#endif
/* I2C2 PB10/PB11 or I2C3 PA8/PC9; each peripheral has one owner. */
tp_result tp_f407_i2c_init_on(tp_f407_i2c *bus, unsigned peripheral);
/* Existing reference defaults to I2C2. */
tp_result tp_f407_i2c_init(tp_f407_i2c *bus);
#ifdef TP_F407_I2C_POLL_ONLY
/* Wait for STOP to finish, then disable the selected bus and release its pins. */
tp_result tp_f407_i2c_poll_shutdown(tp_f407_i2c *bus);
#endif
tp_i2c_io tp_f407_i2c_io(tp_f407_i2c *bus);
#endif
