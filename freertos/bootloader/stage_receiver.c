#include "tp_stage_receiver.h"
#include <string.h>

static bool same(const tp_download_manifest *a,const tp_download_manifest *b) {
    return a->bank==b->bank && a->version==b->version && a->size==b->size &&
        !memcmp(a->id,b->id,sizeof(a->id));
}
static tp_result erase(void *v,uint8_t bank) {
    tp_stage_receiver *s=v; return s->storage.erase_bank(s->storage.image_ctx,bank);
}
static tp_result read_image(void *v,uint8_t bank,uint32_t off,void *p,size_t n) {
    tp_stage_receiver *s=v; return s->storage.read_bank(s->storage.image_ctx,bank,off,p,n);
}
static tp_result write_image(void *v,uint8_t bank,uint32_t off,const void *p,size_t n) {
    tp_stage_receiver *s=v; return s->storage.write_bank(s->storage.image_ctx,bank,off,p,n);
}
static tp_result verify(void *v,const tp_download_manifest *m) {
    tp_stage_receiver *s=v; return tp_signed_manifest_verify_image(&s->signature,m);
}
static tp_result eligible(tp_stage_receiver *s) {
    tp_boot_state b; tp_result r=tp_boot_read(&s->boot,&b);
    if(r!=TP_OK) return r;
    if(b.phase!=TP_BOOT_IDLE) return TP_BUSY;
    return s->manifest.bank!=b.active_bank && s->manifest.version>b.active_version ? TP_OK : TP_INVALID;
}
static tp_result check(tp_stage_receiver *s) {
    if(!s || !s->ready) return TP_INVALID;
    tp_result r=eligible(s); if(r!=TP_OK) return r;
    tp_download_state state; r=tp_download_read(&s->download,&state);
    if(r!=TP_OK) return r;
    return same(&state.manifest,&s->manifest) ? TP_OK : TP_INVALID;
}
static tp_result init_receiver(tp_stage_receiver *s,const tp_boot_io *boot,
        const tp_download_io *storage,const uint32_t capacity[2],const uint8_t key[64],
        uint32_t product,const uint8_t *bytes,size_t n) {
    if(!s || !boot || !storage || !capacity || !key || !bytes ||
       !storage->erase_bank || !storage->read_bank || !storage->write_bank) return TP_INVALID;
    memset(s,0,sizeof(*s)); s->boot=*boot; s->storage=*storage;
    s->signature.image=(tp_image_hash_io){s,read_image,{capacity[0],capacity[1]}};
    s->signature.product_id=product;
    memcpy(s->signature.public_key,key,64);
    s->download=(tp_download_io){storage->metadata,s,erase,read_image,write_image,verify};
    tp_result r=tp_signed_manifest_authenticate(&s->signature,bytes,n,&s->manifest);
    if(r!=TP_OK || (r=eligible(s))!=TP_OK) return r;
    return r;
}
tp_result tp_stage_receiver_cancel_manifest(tp_stage_receiver *s,const tp_boot_io *boot,
        const tp_download_io *storage,const uint32_t capacity[2],const uint8_t key[64],
        uint32_t product,const uint8_t *bytes,size_t n) {
    tp_result r=init_receiver(s,boot,storage,capacity,key,product,bytes,n);
    tp_download_state state;
    if(r==TP_OK) r=tp_download_read(&s->download,&state);
    if(r==TP_OK && !same(&state.manifest,&s->manifest)) r=TP_INVALID;
    if(r==TP_OK) r=tp_download_cancel(&s->download);
    return r;
}
tp_result tp_stage_receiver_open(tp_stage_receiver *s,const tp_boot_io *boot,
        const tp_download_io *storage,const uint32_t capacity[2],const uint8_t key[64],
        uint32_t product,const uint8_t *bytes,size_t n) {
    tp_result r=init_receiver(s,boot,storage,capacity,key,product,bytes,n);
    if(r!=TP_OK) return r;
    tp_download_state state; r=tp_download_read(&s->download,&state);
    if(r==TP_OK && state.phase!=TP_DOWNLOAD_CANCELLED && same(&state.manifest,&s->manifest)) {
        if(state.phase==TP_DOWNLOAD_VERIFIED) r=verify(s,&s->manifest);
        else r=tp_download_resume(&s->download);
    } else if(r==TP_OK || r==TP_INVALID) {
        r=tp_download_begin(&s->download,&s->manifest);
    }
    if(r==TP_OK) s->ready=true;
    return r;
}
tp_result tp_stage_receiver_cancel(tp_stage_receiver *s) {
    tp_result r=check(s);
    if(r==TP_OK) r=tp_download_cancel(&s->download);
    if(r==TP_OK) s->ready=false;
    return r;
}
tp_result tp_stage_receiver_write(tp_stage_receiver *s,uint32_t offset,const void *data,size_t n) {
    tp_result r=check(s); if(r==TP_OK) r=tp_download_write(&s->download,offset,data,n);
    if(r!=TP_OK && s) s->ready=false;
    return r;
}
tp_result tp_stage_receiver_finish(tp_stage_receiver *s) {
    tp_result r=check(s);
    if(r==TP_OK) {
        tp_download_state state; r=tp_download_read(&s->download,&state);
        if(r==TP_OK && state.phase==TP_DOWNLOAD_VERIFIED) r=verify(s,&s->manifest);
        else if(r==TP_OK) r=tp_download_finish(&s->download);
    }
    if(r!=TP_OK && s) s->ready=false;
    return r;
}
