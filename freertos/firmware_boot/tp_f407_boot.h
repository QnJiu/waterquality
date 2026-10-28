#ifndef TP_F407_BOOT_H
#define TP_F407_BOOT_H
#include "tp_boot_target.h"
#include "tp_upgrade_fram.h"
#include "tp_signed_boot.h"
#include "tp_f407_image_flash.h"
/* Board supplies polling I2C and read-only image access; contexts stay alive.
 * Public key and product ID must come from a trusted immutable source. */
typedef struct {
    tp_i2c_io fram_bus;
    void *image_ctx;
    tp_result (*image_read)(void *,uint8_t,uint32_t,void *,size_t);
    uint8_t public_key[64];
    uint32_t product_id;
} tp_f407_boot_options;
typedef struct {
    tp_upgrade_fram fram;
    tp_signed_boot signed_boot;
    tp_boot_target_config target;
} tp_f407_boot;
tp_result tp_f407_boot_init(tp_f407_boot *,const tp_f407_boot_options *);
tp_result tp_f407_boot_prepare(tp_f407_boot *,tp_boot_target *);
/* Trusted factory only: image is already programmed; exclusive access required.
 * Authenticates and checks vectors before any factory metadata writes. */
tp_result tp_f407_boot_factory_provision(tp_f407_boot *,const uint8_t *manifest,size_t size);
#endif
