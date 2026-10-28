#include "tp_upgrade_commands.h"
#include <string.h>

tp_result tp_upgrade_command_execute(const tp_upgrade_commands *c,tp_upgrade_command command,
                                     const uint8_t *data,size_t n,tp_upgrade_reply *reply) {
    if(!reply) return TP_INVALID;
    memset(reply,0,sizeof(*reply)); reply->result=TP_INVALID;
    if(!c || !c->upgrade || !c->upgrade->boot || !c->upgrade->download ||
       !c->authenticate || (n && !data)) return TP_INVALID;
    tp_result r=TP_INVALID;
    switch(command) {
    case TP_UP_CMD_STATUS:
        if(!n) r=TP_OK;
        break;
    case TP_UP_CMD_BEGIN:
        if(n) {
            tp_download_manifest manifest={0};
            r=c->authenticate(c->auth_ctx,data,n,&manifest);
            if(r==TP_OK) r=tp_upgrade_begin(c->upgrade,&manifest);
        }
        break;
    case TP_UP_CMD_RESUME:
        if(!n) r=tp_upgrade_resume(c->upgrade);
        break;
    case TP_UP_CMD_WRITE:
        if(n>4 && n<=4+TP_DOWNLOAD_CHUNK_MAX) {
            uint32_t offset=(uint32_t)data[0] | ((uint32_t)data[1]<<8) |
                ((uint32_t)data[2]<<16) | ((uint32_t)data[3]<<24);
            r=tp_upgrade_write(c->upgrade,offset,data+4,n-4);
        }
        break;
    case TP_UP_CMD_FINISH:
        if(!n) r=tp_upgrade_finish(c->upgrade);
        break;
    default: break;
    }
    reply->result=r;
    tp_result state_result=tp_download_read(c->upgrade->download,&reply->state);
    reply->state_available=state_result==TP_OK;
    /* STATUS distinguishes blank/damaged metadata and I/O errors. */
    if(command==TP_UP_CMD_STATUS && r==TP_OK) reply->result=state_result;
    return reply->result;
}
