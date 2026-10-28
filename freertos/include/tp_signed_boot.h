#ifndef TP_SIGNED_BOOT_H
#define TP_SIGNED_BOOT_H
#include "tp_signed_manifest.h"
#include "tp_boot.h"

/* Dedicated per-bank slots, each at least 120 bytes and independently erasable.
 * Never erase a running or staged bank's manifest. All operations require one
 * owner, with image writes stopped during verification and until boot handoff. */
typedef struct {
    tp_signed_manifest signature;
    tp_store_io manifests;
    uint8_t signed_bytes[TP_SIGNED_MANIFEST_SIZE];
    bool has_manifest;
} tp_signed_boot;

/* Command authentication; clears the cached manifest on every failure. */
tp_result tp_signed_boot_authenticate(void *,const uint8_t *,size_t,tp_download_manifest *);
/* Download verify callback: checks image, then writes and verifies its manifest
 * before VERIFIED can be recorded. Only for an eligible inactive bank. */
tp_result tp_signed_boot_download_verify(void *,const tp_download_manifest *);
/* Boot verify callback: always rereads the persisted manifest and image, and
 * independently checks signature, product, bank, version, size and digest. */
tp_result tp_signed_boot_verify(void *,uint8_t,uint32_t);
/* Authenticates and hashes again; size is written only after full success. */
tp_result tp_signed_boot_verify_size(void *,uint8_t,uint32_t,uint32_t *size);
#endif
