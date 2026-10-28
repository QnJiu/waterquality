#ifndef TP_BOOT_H
#define TP_BOOT_H
#include "tp_store.h"
/* Metadata uses a dedicated two-slot store, each slot >= 64 bytes. Erases must
 * not affect image banks, application settings or the other metadata slot. */
typedef enum { TP_BOOT_IDLE, TP_BOOT_READY, TP_BOOT_TRIAL } tp_boot_phase;
typedef struct {
    uint32_t sequence, active_version, candidate_version;
    uint8_t active_bank, candidate_bank;
    tp_boot_phase phase;
} tp_boot_state;
typedef struct {
    tp_store_io metadata;
    void *image_ctx;
    /* TP_OK only after checking bank bounds, compatibility, authenticity and
     * integrity against a trusted key and the exact requested version.
     * TP_INVALID means invalid image; I/O errors must remain distinguishable. */
    tp_result (*verify_image)(void *ctx,uint8_t bank,uint32_t version);
} tp_boot_io;
/* Explicit manufacturing initialization of a blank dedicated metadata store.
 * Both record areas must be erased to 0xff. A valid or damaged record is
 * never overwritten. Must be called only in trusted setup. */
tp_result tp_boot_provision(const tp_boot_io *,uint8_t active_bank,uint32_t version);
tp_result tp_boot_read(const tp_boot_io *,tp_boot_state *);
/* Stage only the inactive, fully downloaded and verified bank; version increases. */
tp_result tp_boot_stage(const tp_boot_io *,uint8_t bank,uint32_t version);
/* Called once per boot. Records TRIAL before choosing candidate. A subsequent
 * reset without confirmation reverts to the previous confirmed bank. */
tp_result tp_boot_select(const tp_boot_io *,uint8_t *bank);
/* Only the trial image may confirm itself after its application health checks. */
tp_result tp_boot_confirm(const tp_boot_io *,uint8_t running_bank);
#endif
