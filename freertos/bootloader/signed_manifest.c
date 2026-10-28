#include "tp_signed_manifest.h"
#include <tinycrypt/sha256.h>
#include <tinycrypt/ecc_dsa.h>
#include <tinycrypt/constants.h>
#include <string.h>

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
tp_result tp_signed_manifest_authenticate(void *context,const uint8_t *data,size_t n,
                                          tp_download_manifest *out) {
    tp_signed_manifest *ctx=context;
    if (!ctx) return TP_INVALID;
    ctx->ready=false;
    if (!data || !out || n!=TP_SIGNED_MANIFEST_SIZE || memcmp(data,"TPU1",4) ||
        le32(data+4)!=ctx->product_id || !ctx->product_id || !le32(data+8) ||
        !le32(data+12) || data[16]>1 || data[17] || data[18] || data[19] ||
        le32(data+12)>ctx->image.capacity[data[16]] || !ctx->image.read) return TP_INVALID;
    uint8_t digest[32]; struct tc_sha256_state_struct hash;
    if (tc_sha256_init(&hash)!=TC_CRYPTO_SUCCESS ||
        tc_sha256_update(&hash,data,TP_SIGNED_MANIFEST_BODY_SIZE)!=TC_CRYPTO_SUCCESS ||
        tc_sha256_final(digest,&hash)!=TC_CRYPTO_SUCCESS) return TP_IO_ERROR;
    uECC_Curve curve=uECC_secp256r1();
    if (uECC_valid_public_key(ctx->public_key,curve)!=0 ||
        uECC_verify(ctx->public_key,digest,sizeof(digest),data+TP_SIGNED_MANIFEST_BODY_SIZE,curve)!=1)
        return TP_INVALID;
    tp_download_manifest m={0};
    memcpy(m.id,digest,sizeof(m.id)); m.version=le32(data+8); m.size=le32(data+12); m.bank=data[16];
    ctx->authenticated=m; memcpy(ctx->image_digest,data+20,32); ctx->ready=true; *out=m;
    return TP_OK;
}
tp_result tp_signed_manifest_verify_image(void *context,const tp_download_manifest *m) {
    tp_signed_manifest *ctx=context;
    if (!ctx || !m || !ctx->ready || m->bank!=ctx->authenticated.bank ||
        m->version!=ctx->authenticated.version || m->size!=ctx->authenticated.size ||
        memcmp(m->id,ctx->authenticated.id,sizeof(m->id))) return TP_INVALID;
    return tp_image_sha256_verify(&ctx->image,m->bank,m->size,ctx->image_digest);
}
