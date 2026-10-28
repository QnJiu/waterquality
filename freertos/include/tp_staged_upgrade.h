#ifndef TP_STAGED_UPGRADE_H
#define TP_STAGED_UPGRADE_H
#include "tp_secure_upgrade.h"
#include "tp_signed_staged_source.h"

/* One owner must exclusively hold boot/download metadata, both image slots,
 * manifest storage and the source for the entire session. Do not move this
 * object. The running image must be the confirmed active slot. */
typedef struct {
    tp_secure_upgrade *upgrade;
    tp_signed_staged_source source;
    tp_staged_copy copy;
    uint8_t signed_bytes[TP_SIGNED_MANIFEST_SIZE];
    bool ready, copied;
} tp_staged_upgrade;
/* Authenticate source and destination contexts before any erase. Source image
 * and reader must stay unchanged until the session is finished. Reinitialize
 * with the same signed bytes after reset to resume. */
tp_result tp_staged_upgrade_init(tp_staged_upgrade *,tp_secure_upgrade *,
                                 const tp_image_hash_io *,const uint8_t *,size_t);
/* Copy at most one chunk per call. TP_OK means target has been verified. */
tp_result tp_staged_upgrade_step(tp_staged_upgrade *);
/* Explicitly stage the verified target for trial boot; does not reset. */
tp_result tp_staged_upgrade_finish(tp_staged_upgrade *);
#endif
