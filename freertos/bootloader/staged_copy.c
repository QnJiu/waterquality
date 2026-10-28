#include "tp_staged_copy.h"
#include <string.h>
static bool same(const tp_download_manifest *a,const tp_download_manifest *b) {
    return a->bank==b->bank && a->size==b->size && a->version==b->version &&
           !memcmp(a->id,b->id,sizeof(a->id));
}
tp_result tp_staged_copy_init(tp_staged_copy *copy,const tp_staged_source *source,
                              const tp_download_io *dest,const tp_download_manifest *manifest) {
    if(!copy) return TP_INVALID;
    memset(copy,0,sizeof(*copy));
    if(!source || !source->read || !source->verify || !dest || !manifest ||
       manifest->bank>1 || !manifest->version || !manifest->size) return TP_INVALID;
    tp_result r=source->verify(source->ctx,manifest);
    if(r!=TP_OK) return r;
    tp_download_state state;
    r=tp_download_read(dest,&state);
    if(r==TP_OK) {
        if(!same(&state.manifest,manifest)) return TP_BUSY;
    } else if(r==TP_INVALID) {
        /* begin rejects nonblank or corrupt metadata; it records ERASING first. */
        r=tp_download_begin(dest,manifest);
        if(r!=TP_OK) return r;
    } else return r;
    copy->source=*source; copy->destination=dest; copy->manifest=*manifest; copy->ready=true;
    return TP_OK;
}
tp_result tp_staged_copy_step(tp_staged_copy *copy) {
    if(!copy || !copy->ready) return TP_INVALID;
    tp_download_state state;
    tp_result r=tp_download_read(copy->destination,&state);
    if(r!=TP_OK || !same(&state.manifest,&copy->manifest)) {
        copy->ready=false; return r!=TP_OK ? r : TP_INVALID;
    }
    if(state.phase==TP_DOWNLOAD_VERIFIED) {
        /* A checkpoint is not proof that flash stayed intact across reset. */
        r=copy->destination->verify(copy->destination->image_ctx,&copy->manifest);
        if(r!=TP_OK) copy->ready=false;
        return r;
    }
    if(state.phase==TP_DOWNLOAD_ERASING) {
        r=tp_download_resume(copy->destination);
        if(r!=TP_OK) copy->ready=false;
        return r==TP_OK ? TP_BUSY : r;
    }
    if(state.phase!=TP_DOWNLOAD_WRITING || state.written>state.manifest.size) {
        copy->ready=false; return TP_INVALID;
    }
    if(state.written==state.manifest.size) {
        r=tp_download_finish(copy->destination);
        if(r!=TP_OK) copy->ready=false;
        return r;
    }
    uint8_t buffer[TP_DOWNLOAD_CHUNK_MAX];
    uint32_t remaining=state.manifest.size-state.written;
    size_t n=remaining<sizeof(buffer) ? remaining : sizeof(buffer);
    r=copy->source.read(copy->source.ctx,state.manifest.bank,state.written,buffer,n);
    if(r==TP_OK) r=tp_download_write(copy->destination,state.written,buffer,n);
    if(r!=TP_OK) { copy->ready=false; return r; }
    return TP_BUSY;
}
