#ifndef TP_DOWNLOAD_H
#define TP_DOWNLOAD_H
#include "tp_store.h"
#define TP_DOWNLOAD_ID_SIZE 32u
#define TP_DOWNLOAD_CHUNK_MAX 256u
typedef enum { TP_DOWNLOAD_ERASING=1, TP_DOWNLOAD_WRITING=2, TP_DOWNLOAD_VERIFIED=3 } tp_download_phase;
typedef struct {
    uint8_t id[TP_DOWNLOAD_ID_SIZE]; /* digest of authenticated manifest */
    uint32_t version, size; uint8_t bank;
} tp_download_manifest;
typedef struct {
    tp_download_manifest manifest;
    uint32_t sequence, written;
    tp_download_phase phase;
} tp_download_state;
typedef struct {
    /* Dedicated independent dual-slot metadata; at least 128 bytes per slot. */
    tp_store_io metadata;
    void *image_ctx;
    tp_result (*erase_bank)(void *,uint8_t);
    tp_result (*read_bank)(void *,uint8_t,uint32_t,void *,size_t);
    /* Must tolerate repeating the same bytes after a partial write. */
    tp_result (*write_bank)(void *,uint8_t,uint32_t,const void *,size_t);
    /* Verifies whole bank image against authenticated manifest ID/version/size. */
    tp_result (*verify)(void *,const tp_download_manifest *);
} tp_download_io;
/* Single owner; caller must ensure the bank is not active or staged for boot.
 * All metadata and image operations may block and must run outside control task. */
tp_result tp_download_read(const tp_download_io *,tp_download_state *);
/* Starts on blank metadata or after a different VERIFIED session. Repeating
 * the same verified manifest returns TP_BUSY without erasing the image. */
tp_result tp_download_begin(const tp_download_io *,const tp_download_manifest *);
/* Repeats interrupted erase before accepting any payload. */
tp_result tp_download_resume(const tp_download_io *);
/* Sequential chunks only, each <= TP_DOWNLOAD_CHUNK_MAX; writes and verifies
 * bytes before checkpointing. On error reload state; retry with identical bytes. */
tp_result tp_download_write(const tp_download_io *,uint32_t offset,const void *,size_t);
/* Verifies the complete image before persisting VERIFIED. Does not stage boot. */
tp_result tp_download_finish(const tp_download_io *);
#endif
