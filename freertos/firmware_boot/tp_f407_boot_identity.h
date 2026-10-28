#ifndef TP_F407_BOOT_IDENTITY_H
#define TP_F407_BOOT_IDENTITY_H
#include <stdint.h>
/* Supplied by the trusted deployment; raw P-256 X || Y, big-endian. */
extern const uint32_t tp_f407_boot_product_id;
extern const uint8_t tp_f407_boot_public_key[64];
#endif
