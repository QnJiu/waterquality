#include "tp_signed_boot.h"
#include <string.h>

#define RECORD_SIZE (TP_SIGNED_MANIFEST_SIZE + 4u)
static const uint8_t commit[4]={0x54,0x50,0x4d,0x31};

static bool valid_store(const tp_store_io *io) {
    return io && io->read && io->erase && io->write;
}

tp_result tp_signed_boot_authenticate(void *context,const uint8_t *data,size_t n,
                                      tp_download_manifest *manifest) {
    tp_signed_boot *ctx=context;
    if(!ctx) return TP_INVALID;
    ctx->has_manifest=false;
    tp_result r=tp_signed_manifest_authenticate(&ctx->signature,data,n,manifest);
    if(r==TP_OK) { memcpy(ctx->signed_bytes,data,TP_SIGNED_MANIFEST_SIZE); ctx->has_manifest=true; }
    return r;
}

tp_result tp_signed_boot_download_verify(void *context,const tp_download_manifest *manifest) {
    tp_signed_boot *ctx=context;
    if(!ctx || !ctx->has_manifest || !manifest || !valid_store(&ctx->manifests)) return TP_INVALID;
    tp_result r=tp_signed_manifest_verify_image(&ctx->signature,manifest);
    if(r!=TP_OK) return r;
    const tp_store_io *io=&ctx->manifests;
    unsigned bank=manifest->bank;
    uint8_t record[RECORD_SIZE];
    r=io->read(io->ctx,bank,0,record,sizeof(record));
    if(r!=TP_OK) return r;
    if(!memcmp(record,ctx->signed_bytes,TP_SIGNED_MANIFEST_SIZE) &&
       !memcmp(record+TP_SIGNED_MANIFEST_SIZE,commit,sizeof(commit))) return TP_OK;
    if(io->erase(io->ctx,bank)!=TP_OK ||
       io->write(io->ctx,bank,0,ctx->signed_bytes,TP_SIGNED_MANIFEST_SIZE)!=TP_OK ||
       io->read(io->ctx,bank,0,record,TP_SIGNED_MANIFEST_SIZE)!=TP_OK ||
       memcmp(record,ctx->signed_bytes,TP_SIGNED_MANIFEST_SIZE)) return TP_IO_ERROR;
    if(io->write(io->ctx,bank,TP_SIGNED_MANIFEST_SIZE,commit,sizeof(commit))!=TP_OK ||
       io->read(io->ctx,bank,0,record,sizeof(record))!=TP_OK ||
       memcmp(record,ctx->signed_bytes,TP_SIGNED_MANIFEST_SIZE) ||
       memcmp(record+TP_SIGNED_MANIFEST_SIZE,commit,sizeof(commit))) return TP_IO_ERROR;
    return TP_OK;
}

tp_result tp_signed_boot_verify_size(void *context,uint8_t bank,uint32_t version,uint32_t *size) {
    tp_signed_boot *ctx=context;
    if(!ctx || !size || bank>1 || !version || !ctx->manifests.read) return TP_INVALID;
    uint8_t record[RECORD_SIZE];
    tp_result r=ctx->manifests.read(ctx->manifests.ctx,bank,0,record,sizeof(record));
    if(r!=TP_OK) return r;
    if(memcmp(record+TP_SIGNED_MANIFEST_SIZE,commit,sizeof(commit))) return TP_INVALID;
    /* A fresh context makes this independent of the download session cache. */
    tp_signed_manifest verifier={0};
    memcpy(verifier.public_key,ctx->signature.public_key,sizeof(verifier.public_key));
    verifier.product_id=ctx->signature.product_id;
    verifier.image=ctx->signature.image;
    tp_download_manifest manifest;
    r=tp_signed_manifest_authenticate(&verifier,record,TP_SIGNED_MANIFEST_SIZE,&manifest);
    if(r!=TP_OK) return r;
    if(manifest.bank!=bank || manifest.version!=version) return TP_INVALID;
    r=tp_signed_manifest_verify_image(&verifier,&manifest);
    if(r==TP_OK) *size=manifest.size;
    return r;
}
tp_result tp_signed_boot_verify(void *context,uint8_t bank,uint32_t version) {
    uint32_t size; return tp_signed_boot_verify_size(context,bank,version,&size);
}
