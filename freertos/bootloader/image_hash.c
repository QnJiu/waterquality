#include "tp_image_hash.h"
#include <tinycrypt/sha256.h>
#include <tinycrypt/constants.h>

tp_result tp_image_sha256(const tp_image_hash_io *io,uint8_t bank,uint32_t size,
                          uint8_t out[TP_IMAGE_SHA256_SIZE]) {
    if (!io || !io->read || !out || bank>1 || !size || size>io->capacity[bank]) return TP_INVALID;
    struct tc_sha256_state_struct hash;
    uint8_t buffer[256];
    if (tc_sha256_init(&hash)!=TC_CRYPTO_SUCCESS) return TP_IO_ERROR;
    for (uint32_t offset=0; offset<size;) {
        uint32_t remaining=size-offset;
        size_t n=remaining<sizeof(buffer) ? remaining : sizeof(buffer);
        tp_result r=io->read(io->ctx,bank,offset,buffer,n);
        if (r!=TP_OK) return r;
        if (tc_sha256_update(&hash,buffer,n)!=TC_CRYPTO_SUCCESS) return TP_IO_ERROR;
        offset+=(uint32_t)n;
    }
    return tc_sha256_final(out,&hash)==TC_CRYPTO_SUCCESS ? TP_OK : TP_IO_ERROR;
}
tp_result tp_image_sha256_verify(const tp_image_hash_io *io,uint8_t bank,uint32_t size,
                                 const uint8_t expected[TP_IMAGE_SHA256_SIZE]) {
    if (!expected) return TP_INVALID;
    uint8_t digest[TP_IMAGE_SHA256_SIZE], difference=0;
    tp_result r=tp_image_sha256(io,bank,size,digest);
    if (r!=TP_OK) return r;
    for (size_t i=0;i<sizeof(digest);++i) difference|=(uint8_t)(digest[i]^expected[i]);
    return difference ? TP_INVALID : TP_OK;
}
