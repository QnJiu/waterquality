#ifndef TP_F407_IMAGE_FLASH_H
#define TP_F407_IMAGE_FLASH_H
#include "tp_core.h"

/* STM32F407VG 1 MiB reference: bank 0 uses sectors 5..7, bank 1 8..10.
 * Sectors 0..4 and 11 are never touched. Revalidate device/layout before use. */
#define TP_F407_IMAGE_BANK_SIZE (384u * 1024u)
#define TP_F407_IMAGE0_BASE 0x08020000u
#define TP_F407_IMAGE1_BASE 0x08080000u

typedef struct {
    void *ctx;
    tp_result (*read)(void *,uint32_t,void *,size_t);
    tp_result (*erase_sector)(void *,unsigned);
    tp_result (*program_byte)(void *,uint32_t,uint8_t);
} tp_f407_flash_ops;
typedef struct {
    tp_f407_flash_ops ops;
    uint8_t active_bank;
} tp_f407_image_flash;
/* Initialize once with trusted active boot bank, then serialize all accesses.
 * Update active_bank only from verified boot state, not client input. */
tp_result tp_f407_image_flash_init(tp_f407_image_flash *,const tp_f407_flash_ops *,uint8_t active_bank);
tp_result tp_f407_image_flash_set_active(tp_f407_image_flash *,uint8_t active_bank);
/* Compatible with tp_secure_upgrade_config image callbacks. */
tp_result tp_f407_image_read(void *,uint8_t,uint32_t,void *,size_t);
tp_result tp_f407_image_erase(void *,uint8_t);
tp_result tp_f407_image_write(void *,uint8_t,uint32_t,const void *,size_t);
#endif
