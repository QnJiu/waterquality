#include "tp_staged_guard.h"
#include <string.h>
tp_result tp_staged_guard_init(tp_staged_guard *g,tp_secure_upgrade *u,void *ctx,
    tp_result (*maintenance)(void *),bool (*lock)(void *),void (*unlock)(void *)) {
    if(!g || !u || !maintenance || !lock || !unlock) return TP_INVALID;
    memset(g,0,sizeof(*g)); g->upgrade=u; g->ctx=ctx; g->maintenance=maintenance;
    g->lock=lock; g->unlock=unlock; return TP_OK;
}
tp_result tp_staged_guard_begin(tp_staged_guard *g,const tp_image_hash_io *image,
                                 const uint8_t *bytes,size_t n) {
    if(!g || !g->upgrade || !g->maintenance || !g->lock || !g->unlock) return TP_INVALID;
    if(g->active) return TP_BUSY;
    tp_result r=g->maintenance(g->ctx); if(r!=TP_OK) return r;
    if(!g->lock(g->ctx)) return TP_LOCK_BUSY;
    g->active=true;
    r=tp_staged_upgrade_init(&g->staged,g->upgrade,image,bytes,n);
    if(r!=TP_OK) tp_staged_guard_abort(g);
    return r;
}
tp_result tp_staged_guard_step(tp_staged_guard *g) {
    if(!g || !g->active) return TP_INVALID;
    tp_result r=tp_staged_upgrade_step(&g->staged);
    if(r!=TP_OK && r!=TP_BUSY) tp_staged_guard_abort(g);
    return r;
}
tp_result tp_staged_guard_finish(tp_staged_guard *g) {
    if(!g || !g->active) return TP_INVALID;
    tp_result r=tp_staged_upgrade_finish(&g->staged);
    if(r!=TP_BUSY) tp_staged_guard_abort(g);
    return r;
}
void tp_staged_guard_abort(tp_staged_guard *g) {
    if(g && g->active) { g->active=false; g->staged.ready=false; g->unlock(g->ctx); }
}
