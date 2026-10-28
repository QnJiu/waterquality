#ifndef TP_STAGED_COPY_H
#define TP_STAGED_COPY_H
#include "tp_download.h"
/* Source verification must authenticate the manifest and the complete image.
 * Source, destination, metadata and the signed manifest must remain immutable
 * and under one exclusive owner until copying and target verification finish.
 * Destination callbacks must protect the active and pending boot slots. */
typedef struct {
    void *ctx;
    tp_result (*verify)(void *,const tp_download_manifest *);
    tp_result (*read)(void *,uint8_t,uint32_t,void *,size_t);
} tp_staged_source;
typedef struct {
    tp_staged_source source;
    const tp_download_io *destination;
    tp_download_manifest manifest;
    bool ready;
} tp_staged_copy;
/* Verifies source before changing the destination. Existing destination state
 * must be blank or match the entire manifest. Reinitialize after an error or
 * reset; the persistent download record resumes interrupted erase/write. */
tp_result tp_staged_copy_init(tp_staged_copy *,const tp_staged_source *,
                              const tp_download_io *,const tp_download_manifest *);
/* One chunk (at most 256 bytes) per call. BUSY means call again; OK means the
 * destination has been completely verified. Does not stage or boot the image.
 * Erase and verify callbacks may block; use only a suitable background task. */
tp_result tp_staged_copy_step(tp_staged_copy *);
#endif
