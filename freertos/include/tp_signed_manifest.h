#ifndef TP_SIGNED_MANIFEST_H
#define TP_SIGNED_MANIFEST_H
#include "tp_image_hash.h"
#define TP_SIGNED_MANIFEST_SIZE 116u
#define TP_SIGNED_MANIFEST_BODY_SIZE 52u
/* Public key is an uncompressed P-256 point x||y, 32-byte big-endian each.
 * Key and policy must be installed through a trusted mechanism. */
typedef struct {
    uint8_t public_key[64];
    uint32_t product_id;
    tp_image_hash_io image;
    tp_download_manifest authenticated;
    uint8_t image_digest[32];
    bool ready;
} tp_signed_manifest;
/* Signature covers bytes 0..51: "TPU1", product/version/size (u32 LE),
 * bank (u8), three zero bytes, image SHA-256. The signature is r||s (64
 * bytes, big-endian). Manifest id is SHA-256 of the 52 signed bytes. */
tp_result tp_signed_manifest_authenticate(void *,const uint8_t *,size_t,tp_download_manifest *);
/* Callback for tp_download_io.verify; image_ctx must be tp_signed_manifest.
 * Reauthenticate the same manifest after reboot before attempting FINISH. */
tp_result tp_signed_manifest_verify_image(void *,const tp_download_manifest *);
#endif
