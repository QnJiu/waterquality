#ifndef TP_F030_I2C_H
#define TP_F030_I2C_H
#include "tp_pn532_i2c.h"

typedef enum { TP_F030_I2C_IDLE, TP_F030_I2C_WRITE, TP_F030_I2C_READ,
    TP_F030_I2C_HELD, TP_F030_I2C_STOPPING, TP_F030_I2C_FAILED } tp_f030_i2c_state;
typedef struct {
    tp_f030_i2c_state state;
    const uint8_t *write_data;
    uint8_t read_data[TP_PN532_FRAME_MAX];
    size_t length, position, segment_end;
    bool final;
} tp_f030_i2c;

/* Exclusive I2C1 ownership. PB6/PB7 AF1, 8 MHz HSI, 100 kHz reference timing. */
tp_result tp_f030_i2c_init(tp_f030_i2c *);
tp_pn532_i2c_bus tp_f030_i2c_bus(tp_f030_i2c *);
#endif
