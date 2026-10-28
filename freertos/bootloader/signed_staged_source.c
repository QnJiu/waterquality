#include "tp_signed_staged_source.h"

static tp_result verify(void *v,const tp_download_manifest *expected) {
    tp_signed_staged_source *source=v;
    tp_download_manifest authenticated;
    if (!source || !expected) return TP_INVALID;
    tp_result r=tp_signed_manifest_authenticate(&source->signature,source->manifest_bytes,
                                                 source->manifest_size,&authenticated);
    if (r!=TP_OK) return r;
    return tp_signed_manifest_verify_image(&source->signature,expected);
}
static tp_result read_source(void *v,uint8_t bank,uint32_t offset,void *out,size_t n) {
    tp_signed_staged_source *source=v;
    if (!source || !source->signature.ready || !source->signature.image.read) return TP_INVALID;
    return source->signature.image.read(source->signature.image.ctx,bank,offset,out,n);
}
tp_staged_source tp_signed_staged_source_callbacks(tp_signed_staged_source *source) {
    return (tp_staged_source){source,verify,read_source};
}
