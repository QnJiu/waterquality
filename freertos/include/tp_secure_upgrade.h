#ifndef TP_SECURE_UPGRADE_H
#define TP_SECURE_UPGRADE_H
#include "tp_signed_boot.h"
#include "tp_upgrade_commands.h"

/* Each metadata store must use two dedicated, independently erasable slots;
 * image and manifest storage must also be independent of all metadata. */
typedef struct {
    tp_store_io boot_metadata, download_metadata, manifests;
    void *image_ctx;
    tp_result (*erase_image)(void *,uint8_t);
    tp_result (*read_image)(void *,uint8_t,uint32_t,void *,size_t);
    tp_result (*write_image)(void *,uint8_t,uint32_t,const void *,size_t);
    uint32_t capacity[2], product_id;
    uint8_t public_key[64];
} tp_secure_upgrade_config;

/* Object and config callbacks must remain valid for the entire session. Do not
 * copy/move the initialized object. Calls and all underlying I/O require one
 * owner; no image or metadata writes between boot verification and handoff. */
typedef struct {
    tp_secure_upgrade_config config;
    tp_signed_boot signed_boot;
    tp_boot_io boot;
    tp_download_io download;
    tp_upgrade upgrade;
    tp_upgrade_commands commands;
} tp_secure_upgrade;

tp_result tp_secure_upgrade_init(tp_secure_upgrade *,const tp_secure_upgrade_config *);
#endif
