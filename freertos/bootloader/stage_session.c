#include "tp_stage_session.h"
#include <string.h>

tp_result tp_stage_session_init(tp_stage_session *s,tp_secure_upgrade *u,const tp_download_io *storage,
    const uint32_t capacity[2],void *ctx,bool (*lock)(void *),void (*unlock)(void *),
    tp_result (*maintenance)(void *)) {
    if(!s || !u || !storage || !capacity || !lock || !unlock || !maintenance ||
       !capacity[0] || !capacity[1]) return TP_INVALID;
    memset(s,0,sizeof(*s));
    tp_result r=tp_stage_commands_init(&s->commands,&u->boot,storage,capacity,
                                       u->config.public_key,u->config.product_id);
    if(r!=TP_OK) return r;
    s->upgrade=u; s->image=(tp_image_hash_io){storage->image_ctx,storage->read_bank,{capacity[0],capacity[1]}};
    s->ctx=ctx; s->lock=lock; s->unlock=unlock; s->maintenance=maintenance;
    return TP_OK;
}
void tp_stage_session_abort(tp_stage_session *s) {
    if(s && s->locked) {
        s->locked=false; s->installing=false; s->commands.opened=false;
        s->install.ready=false; s->unlock(s->ctx);
    }
}
tp_result tp_stage_session_execute(void *ctx,const tp_upgrade_commands *unused,tp_upgrade_command cmd,
    const uint8_t *data,size_t n,tp_upgrade_reply *reply) {
    tp_stage_session *s=ctx;
    if(!reply) return TP_INVALID;
    memset(reply,0,sizeof(*reply)); reply->result=TP_INVALID;
    if(!s || !s->upgrade || !s->lock || s->installing || (n && !data)) return TP_INVALID;
    if(cmd==TP_UP_CMD_BEGIN) {
        if(n!=TP_SIGNED_MANIFEST_SIZE) return TP_INVALID;
        if(!s->locked) {
            if(!s->lock(s->ctx)) { reply->result=TP_LOCK_BUSY; return TP_LOCK_BUSY; }
            s->locked=true;
        }
    } else if(!s->locked) return TP_INVALID;
    tp_result r=tp_stage_commands_execute(&s->commands,unused,cmd,data,n,reply);
    if(cmd==TP_UP_CMD_BEGIN) {
        if(r==TP_OK) memcpy(s->manifest,data,sizeof(s->manifest));
        else tp_stage_session_abort(s);
    }
    return r;
}
tp_result tp_stage_session_install_begin(tp_stage_session *s) {
    if(!s || !s->locked || s->installing || !s->commands.opened) return TP_INVALID;
    tp_download_state state;
    tp_result r=tp_download_read(&s->commands.receiver.download,&state);
    if(r!=TP_OK) return r;
    if(state.phase!=TP_DOWNLOAD_VERIFIED) return TP_INVALID;
    r=s->maintenance(s->ctx); if(r!=TP_OK) return r;
    r=tp_staged_upgrade_init(&s->install,s->upgrade,&s->image,s->manifest,sizeof(s->manifest));
    if(r==TP_OK) s->installing=true;
    return r;
}
tp_result tp_stage_session_install_step(tp_stage_session *s) {
    if(!s || !s->locked || !s->installing) return TP_INVALID;
    tp_result r=tp_staged_upgrade_step(&s->install);
    if(r==TP_OK) r=tp_staged_upgrade_finish(&s->install);
    if(r!=TP_BUSY) tp_stage_session_abort(s);
    return r;
}
