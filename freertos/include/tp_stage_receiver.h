#ifndef TP_STAGE_RECEIVER_H
#define TP_STAGE_RECEIVER_H
#include "tp_boot.h"
#include "tp_signed_manifest.h"
/* One owner holds the shared upgrade lock and exclusively owns staging storage
 * throughout the session. Metadata must be independent from boot and destination
 * metadata. Operations may block; run them outside the control task. */
typedef struct {
    tp_boot_io boot;
    tp_download_io storage, download;
    tp_signed_manifest signature;
    tp_download_manifest manifest;
    bool ready;
} tp_stage_receiver;
/* Authenticate and check policy before erasing. Resume an identical interrupted
 * session. Rehash an already completed image whenever it is opened. */
tp_result tp_stage_receiver_open(tp_stage_receiver *,const tp_boot_io *,
    const tp_download_io *,const uint32_t capacity[2],const uint8_t public_key[64],
    uint32_t product_id,const uint8_t *manifest,size_t length);
tp_result tp_stage_receiver_write(tp_stage_receiver *,uint32_t offset,const void *,size_t);
/* Verify the complete image without changing boot state. */
tp_result tp_stage_receiver_finish(tp_stage_receiver *);
#endif
