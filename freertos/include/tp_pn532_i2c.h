#ifndef TP_PN532_I2C_H
#define TP_PN532_I2C_H
#include "tp_pn532.h"

#define TP_PN532_I2C_ADDRESS 0x24u
/* All callbacks are bounded and nonblocking. Each accepted operation is polled
 * until completion. A non-final read holds the transaction open; the bus must
 * support continuing it with a new length and a final NACK/STOP. */
typedef struct {
    void *ctx;
    tp_result (*write_start)(void *,uint8_t address,const uint8_t *,size_t);
    tp_result (*write_poll)(void *);
    tp_result (*read_start)(void *,uint8_t address,size_t count,bool final);
    tp_result (*read_continue)(void *,size_t count,bool final);
    tp_result (*read_poll)(void *,uint8_t *,size_t);
    /* Request STOP on a held read after a not-ready status. */
    void (*read_stop)(void *);
    /* Abort active transfer and release the bus; reset the PN532 separately. */
    void (*abort)(void *);
} tp_pn532_i2c_bus;

typedef enum { TP_PN532_I2C_IDLE, TP_PN532_I2C_WRITE, TP_PN532_I2C_STATUS,
    TP_PN532_I2C_HEADER, TP_PN532_I2C_BODY, TP_PN532_I2C_FAULT } tp_pn532_i2c_state;
typedef struct {
    tp_pn532_i2c_bus bus;
    tp_pn532_i2c_state state;
    uint8_t frame[TP_PN532_FRAME_MAX];
    size_t length, remaining;
} tp_pn532_i2c;

/* Initialize only after bus recovery and PN532 reset/synchronization. */
tp_result tp_pn532_i2c_init(tp_pn532_i2c *,const tp_pn532_i2c_bus *);
/* Adapts to tp_pn532; transport and device must have the same single owner. */
tp_pn532_io tp_pn532_i2c_io(tp_pn532_i2c *);
/* Abort after command timeout or before recovering a failed transport. */
void tp_pn532_i2c_abort(tp_pn532_i2c *);
#endif
