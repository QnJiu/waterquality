#include "tp_f407_boot_flash.h"
#include <string.h>
void tp_f407_boot_flash_reference_init(tp_f407_boot_flash *flash) {
    if (!flash) return;
    flash->slot[0]=(const uint8_t *)(uintptr_t)TP_F407_IMAGE0_BASE;
    flash->slot[1]=(const uint8_t *)(uintptr_t)TP_F407_IMAGE1_BASE;
}
tp_result tp_f407_boot_flash_read(void *ctx,uint8_t bank,uint32_t offset,void *out,size_t size) {
    const tp_f407_boot_flash *flash=ctx;
    if (!flash || bank>1 || !flash->slot[bank] || offset>TP_F407_IMAGE_BANK_SIZE ||
        size>TP_F407_IMAGE_BANK_SIZE-offset || (!out && size)) return TP_INVALID;
    if (size) memcpy(out,flash->slot[bank]+offset,size);
    return TP_OK;
}
