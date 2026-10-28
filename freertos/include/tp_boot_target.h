#ifndef TP_BOOT_TARGET_H
#define TP_BOOT_TARGET_H
#include "tp_boot.h"
/* verify must authenticate the exact bank/version and entire image and return
 * its authenticated byte count only on success. No writes may occur between
 * verification and handoff; all operations are serialized by one owner. */
typedef struct {
    tp_store_io metadata;
    void *verify_ctx;
    void *read_ctx;
    tp_result (*verify)(void *,uint8_t,uint32_t,uint32_t *size);
    tp_result (*read)(void *,uint8_t,uint32_t,void *,size_t);
    uint32_t base[2], capacity[2];
    uint32_t ram_start[2], ram_end[2]; /* half-open RAM ranges; stack top may equal end */
    uint32_t vector_alignment; /* power of two */
} tp_boot_target_config;
typedef struct { uint8_t bank; uint32_t vector, stack, entry; } tp_boot_target;
/* Verify, validate initial stack and Thumb entry, and persist trial before
 * returning a target. TP_INVALID candidates roll back via tp_boot_select.
 * Result is usable only while image contents remain immutable. */
tp_result tp_boot_target_prepare(const tp_boot_target_config *,tp_boot_target *);
#endif
