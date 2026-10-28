#ifndef TP_IMAGE_HASH_H
#define TP_IMAGE_HASH_H
#include "tp_download.h"
#define TP_IMAGE_SHA256_SIZE 32u
/* Read exactly size bytes from a bank. Capacity must come from the validated
 * bank layout, and the expected digest from an authenticated manifest.
 * The caller serializes reads with writes and validates bank ownership. */
typedef struct {
    void *ctx;
    tp_result (*read)(void *,uint8_t bank,uint32_t offset,void *,size_t);
    uint32_t capacity[2];
} tp_image_hash_io;
tp_result tp_image_sha256(const tp_image_hash_io *,uint8_t bank,uint32_t size,
                          uint8_t out[TP_IMAGE_SHA256_SIZE]);
/* TP_INVALID indicates bad arguments, size or digest mismatch. Read errors are
 * propagated. This checks integrity only; it does not authenticate the digest. */
tp_result tp_image_sha256_verify(const tp_image_hash_io *,uint8_t bank,uint32_t size,
                                 const uint8_t expected[TP_IMAGE_SHA256_SIZE]);
#endif
