#ifndef TP_PN532_H
#define TP_PN532_H
#include "tp_core.h"

/* Normal PN532 frame: 255 data bytes plus framing. Extended frames unsupported. */
#define TP_PN532_FRAME_MAX 262u
#define TP_PN532_DATA_MAX 253u
typedef struct {
    void *ctx;
    /* Nonblocking: copies the complete frame or reports failure. */
    tp_result (*send)(void *,const uint8_t *,size_t);
    /* TP_BUSY means no frame; TP_OK returns one complete frame without I2C status.
     * On I2C, inspect RDY and read the entire frame before the same STOP. */
    tp_result (*receive)(void *,uint8_t *,size_t,size_t *);
} tp_pn532_io;
typedef enum { TP_PN532_IDLE, TP_PN532_ACK, TP_PN532_RESPONSE,
               TP_PN532_DONE, TP_PN532_DESYNC } tp_pn532_state;
typedef struct {
    tp_pn532_io io;
    tp_pn532_state state;
    uint32_t started_ms, timeout_ms;
    uint8_t command, data[TP_PN532_DATA_MAX];
    size_t data_len;
} tp_pn532;

/* Initialize only after hardware reset or another verified link resynchronization. */
tp_result tp_pn532_init(tp_pn532 *,const tp_pn532_io *);
tp_result tp_pn532_start(tp_pn532 *,uint32_t now,uint32_t timeout_ms,uint8_t command,
                         const uint8_t *parameters,size_t len);
/* TP_BUSY while pending, TP_OK when complete. Any error requires resynchronization. */
tp_result tp_pn532_tick(tp_pn532 *,uint32_t now);
/* Consumes completed response; insufficient capacity leaves it available. */
tp_result tp_pn532_take(tp_pn532 *,uint8_t *data,size_t capacity,size_t *len);

typedef struct { uint8_t target, uid[10], uid_len, sens_res[2], sel_res; } tp_pn532_type_a;
/* Decode InListPassiveTarget response for MaxTg=1, BrTy=0; UID is not authentication. */
tp_result tp_pn532_type_a_parse(const uint8_t *,size_t,bool *found,tp_pn532_type_a *);
#endif
