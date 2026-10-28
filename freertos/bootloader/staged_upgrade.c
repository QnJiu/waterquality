#include "tp_staged_upgrade.h"
#include <string.h>

static tp_result eligible(tp_staged_upgrade *s,const tp_download_manifest *m) {
    tp_boot_state b;
    tp_result r=tp_boot_read(&s->upgrade->boot,&b);
    if(r!=TP_OK) return r;
    if(b.phase!=TP_BOOT_IDLE) return TP_BUSY;
    return m->bank!=b.active_bank && m->version>b.active_version ? TP_OK : TP_INVALID;
}
static bool same(const tp_download_manifest *a,const tp_download_manifest *b) {
    return a->bank==b->bank && a->version==b->version && a->size==b->size &&
           !memcmp(a->id,b->id,sizeof(a->id));
}
tp_result tp_staged_upgrade_init(tp_staged_upgrade *s,tp_secure_upgrade *u,
                                 const tp_image_hash_io *image,const uint8_t *bytes,size_t n) {
    if(!s || !u || !image || !image->read || !bytes || n!=TP_SIGNED_MANIFEST_SIZE)
        return TP_INVALID;
    /* Preserve arguments before clearing the session. */
    tp_image_hash_io source_image=*image;
    uint8_t signed_bytes[TP_SIGNED_MANIFEST_SIZE]; memcpy(signed_bytes,bytes,sizeof(signed_bytes));
    memset(s,0,sizeof(*s)); s->upgrade=u; memcpy(s->signed_bytes,signed_bytes,sizeof(signed_bytes));
    memcpy(s->source.signature.public_key,u->config.public_key,sizeof(u->config.public_key));
    s->source.signature.product_id=u->config.product_id;
    s->source.signature.image=source_image;
    s->source.manifest_bytes=s->signed_bytes; s->source.manifest_size=sizeof(s->signed_bytes);
    tp_download_manifest m;
    tp_result r=tp_signed_boot_authenticate(&u->signed_boot,s->signed_bytes,sizeof(s->signed_bytes),&m);
    if(r!=TP_OK || (r=eligible(s,&m))!=TP_OK) return r;
    tp_staged_source source=tp_signed_staged_source_callbacks(&s->source);
    /* Verify before replacing an earlier completed download record. */
    r=source.verify(source.ctx,&m); if(r!=TP_OK) return r;
    tp_download_state previous;
    r=tp_download_read(&u->download,&previous);
    if(r==TP_OK && !same(&previous.manifest,&m)) {
        if(previous.phase!=TP_DOWNLOAD_VERIFIED) return TP_BUSY;
        r=tp_upgrade_begin(&u->upgrade,&m); if(r!=TP_OK) return r;
    } else if(r!=TP_OK && r!=TP_INVALID) return r;
    r=tp_staged_copy_init(&s->copy,&source,&u->download,&m);
    if(r==TP_OK) s->ready=true;
    return r;
}
tp_result tp_staged_upgrade_step(tp_staged_upgrade *s) {
    if(!s || !s->ready) return TP_INVALID;
    tp_result r=eligible(s,&s->copy.manifest);
    if(r==TP_OK) r=tp_staged_copy_step(&s->copy);
    if(r==TP_OK) s->copied=true;
    else if(r!=TP_BUSY) s->ready=false;
    return r;
}
tp_result tp_staged_upgrade_finish(tp_staged_upgrade *s) {
    if(!s || !s->ready || !s->copied) return TP_INVALID;
    tp_result r=eligible(s,&s->copy.manifest);
    if(r==TP_OK) r=tp_staged_copy_step(&s->copy);
    if(r==TP_OK) r=tp_upgrade_finish(&s->upgrade->upgrade);
    if(r!=TP_BUSY) s->ready=false;
    return r;
}
