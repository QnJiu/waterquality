#ifndef TP_F407_BOOT_FLASH_H
#define TP_F407_BOOT_FLASH_H
#include "tp_core.h"
#include "tp_f407_image_flash.h"
/* Immutable, readable memory for both complete reference slots. The mapping
 * and contents must stay fixed from verification through application handoff. */
typedef struct { const uint8_t *slot[2]; } tp_f407_boot_flash;
void tp_f407_boot_flash_reference_init(tp_f407_boot_flash *flash);
tp_result tp_f407_boot_flash_read(void *ctx, uint8_t bank, uint32_t offset, void *out, size_t size);
#endif
