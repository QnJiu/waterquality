#include "tp_stage_commands.h"
#include <string.h>

tp_result tp_stage_commands_init(tp_stage_commands *s,const tp_boot_io *boot,
    const tp_download_io *storage,const uint32_t capacity[2],const uint8_t key[64],uint32_t product) {
    if(!s || !boot || !storage || !capacity || !key || !product ||
       !storage->erase_bank || !storage->read_bank || !storage->write_bank ||
       !storage->metadata.read || !storage->metadata.write || !storage->metadata.erase)
        return TP_INVALID;
    memset(s,0,sizeof(*s)); s->boot=*boot; s->storage=*storage;
    s->capacity[0]=capacity[0]; s->capacity[1]=capacity[1];
    memcpy(s->public_key,key,64); s->product_id=product; s->initialized=true;
    return TP_OK;
}
tp_result tp_stage_commands_execute(void *ctx,const tp_upgrade_commands *unused,tp_upgrade_command cmd,
    const uint8_t *data,size_t n,tp_upgrade_reply *reply) {
    (void)unused;
    if(!reply) return TP_INVALID;
    memset(reply,0,sizeof(*reply)); reply->result=TP_INVALID;
    tp_stage_commands *s=ctx;
    if(!s || !s->initialized || (n && !data)) return TP_INVALID;
    tp_result r=TP_INVALID;
    switch(cmd) {
    case TP_UP_CMD_STATUS:
        if(!n && s->opened) r=TP_OK;
        break;
    case TP_UP_CMD_BEGIN:
        if(n) {
            s->opened=false;
            r=tp_stage_receiver_open(&s->receiver,&s->boot,&s->storage,s->capacity,
                                      s->public_key,s->product_id,data,n);
            if(r==TP_OK) s->opened=true;
        }
        break;
    case TP_UP_CMD_RESUME:
        /* BEGIN reauthenticates and resumes after reset. */
        if(!n && s->opened && s->receiver.ready) r=TP_OK;
        break;
    case TP_UP_CMD_CANCEL:
        if(!n && s->opened) r=tp_stage_receiver_cancel(&s->receiver);
        else if(n && !s->opened) {
            r=tp_stage_receiver_cancel_manifest(&s->receiver,&s->boot,&s->storage,
                s->capacity,s->public_key,s->product_id,data,n);
            if(r==TP_OK) s->opened=true; /* Include the resulting state in the reply. */
        }
        break;
    case TP_UP_CMD_WRITE:
        if(s->opened && n>4 && n<=4+TP_DOWNLOAD_CHUNK_MAX) {
            uint32_t off=(uint32_t)data[0]|((uint32_t)data[1]<<8)|
                         ((uint32_t)data[2]<<16)|((uint32_t)data[3]<<24);
            r=tp_stage_receiver_write(&s->receiver,off,data+4,n-4);
        }
        break;
    case TP_UP_CMD_FINISH:
        if(!n && s->opened) r=tp_stage_receiver_finish(&s->receiver);
        break;
    default: break;
    }
    reply->result=r;
    if(s->opened) {
        tp_result state=tp_download_read(&s->receiver.download,&reply->state);
        reply->state_available=state==TP_OK;
        if(cmd==TP_UP_CMD_STATUS && r==TP_OK) reply->result=state;
    }
    return reply->result;
}
