#ifndef TP_UPGRADE_WIRE_H
#define TP_UPGRADE_WIRE_H
#include "tp_upgrade_commands.h"

/* Maximum authenticated manifest or write payload. */
#define TP_UP_WIRE_PAYLOAD_MAX 512u
#define TP_UP_WIRE_RAW_MAX (TP_UP_WIRE_PAYLOAD_MAX + 8u)
#define TP_UP_WIRE_ENCODED_MAX (2u * TP_UP_WIRE_RAW_MAX + 2u)
typedef struct {
    const tp_upgrade_commands *commands;
    void *send_ctx;
    void *execute_ctx;
    tp_result (*execute)(void *,const tp_upgrade_commands *,tp_upgrade_command,
                         const uint8_t *,size_t,tp_upgrade_reply *);
    /* Must copy the complete frame before return. May block in upgrade task. */
    tp_result (*send)(void *,const uint8_t *,size_t);
    uint8_t raw[TP_UP_WIRE_RAW_MAX];
    size_t length;
    bool started, escaped, discard;
} tp_upgrade_wire;

tp_result tp_upgrade_wire_init(tp_upgrade_wire *,const tp_upgrade_commands *,void *,
                               tp_result (*send)(void *,const uint8_t *,size_t));
/* Configure before receiving bytes. Executor must always populate reply, even
 * on failure. NULL restores the default command executor. */
void tp_upgrade_wire_set_executor(tp_upgrade_wire *,void *,
    tp_result (*)(void *,const tp_upgrade_commands *,tp_upgrade_command,
                 const uint8_t *,size_t,tp_upgrade_reply *));
/* Deliver ordered bytes from one owner. Frame errors return TP_INVALID, send
 * errors propagate; a delimiter starts a new frame after any error. */
tp_result tp_upgrade_wire_byte(tp_upgrade_wire *,uint8_t);
/* On UART error or timeout discard the partial frame until the next delimiter. */
void tp_upgrade_wire_reset(tp_upgrade_wire *);
/* Host/client helper: encode a request; seq is echoed, not replay protection. */
tp_result tp_upgrade_wire_request(uint8_t command,uint16_t seq,const uint8_t *,size_t,
                                  uint8_t *out,size_t capacity,size_t *length);
#endif
