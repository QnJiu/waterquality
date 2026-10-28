#ifndef TP_F407_UPGRADE_H
#define TP_F407_UPGRADE_H
#include "tp_secure_upgrade.h"
#include "tp_upgrade_fram.h"
#include "tp_f407_image_flash.h"

/* Reference STM32F407VG slot layout. The bus must address the dedicated
 * upgrade FRAM, isolated from any chip at the same address. The trusted
 * running_vector must be obtained from the processor VTOR, never a request.
 * Public key/product identity and flash operations are board supplied. */
typedef struct {
    tp_i2c_io fram_bus;
    tp_f407_flash_ops flash_ops;
    uint32_t running_vector, product_id;
    uint8_t public_key[64];
} tp_f407_upgrade_options;
typedef struct {
    tp_upgrade_fram fram;
    tp_f407_image_flash image;
    tp_secure_upgrade secure;
    uint8_t running_bank;
} tp_f407_upgrade;
/* Initialize once, before concurrent access. Object and I/O contexts must
 * remain alive and must not be moved. An error leaves the object unusable.
 * All subsequent access shares one lock, including confirmation and image
 * writes. F407 single-bank erase/program can stall execution; board software
 * must establish safe outputs and a compatible watchdog strategy first. */
tp_result tp_f407_upgrade_init(tp_f407_upgrade *,const tp_f407_upgrade_options *);
#endif
