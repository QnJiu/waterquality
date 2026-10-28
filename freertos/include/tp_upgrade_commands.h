#ifndef TP_UPGRADE_COMMANDS_H
#define TP_UPGRADE_COMMANDS_H
#include "tp_upgrade.h"

typedef enum { TP_UP_CMD_STATUS=1, TP_UP_CMD_BEGIN=2, TP_UP_CMD_RESUME=3,
               TP_UP_CMD_WRITE=4, TP_UP_CMD_FINISH=5, TP_UP_CMD_CANCEL=6 } tp_upgrade_command;
typedef struct {
    const tp_upgrade *upgrade;
    void *auth_ctx;
    /* Authenticate the entire supplied manifest, check product and bank capacity,
     * and return its durable identity. May block; do not run in a control task. */
    tp_result (*authenticate)(void *,const uint8_t *,size_t,tp_download_manifest *);
} tp_upgrade_commands;
typedef struct {
    tp_result result;
    bool state_available;
    tp_download_state state;
} tp_upgrade_reply;

/* Complete request, already framed and integrity-checked by transport.
 * STATUS/RESUME/FINISH/CANCEL: empty; BEGIN: authenticated manifest bytes;
 * WRITE: 4-byte little-endian offset followed by 1..256 payload bytes.
 * The reply buffer is mandatory and populated even when the operation fails.
 * All upgrade and boot operations must have one serialized owner. */
tp_result tp_upgrade_command_execute(const tp_upgrade_commands *,tp_upgrade_command,
                                     const uint8_t *,size_t,tp_upgrade_reply *);
#endif
