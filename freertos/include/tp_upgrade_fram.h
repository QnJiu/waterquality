#ifndef TP_UPGRADE_FRAM_H
#define TP_UPGRADE_FRAM_H
#include "tp_mb85rc16.h"
/* Dedicated MB85RC16: boot 0..255, download 256..511, manifests 512..767, staging download 768..1023.
 * The chip and all contexts must remain alive and access must be serialized.
 * A second MB85RC16 requires a separate I2C bus or isolated mux channel. */
typedef struct { tp_i2c_io bus; uint16_t base, commit; } tp_upgrade_fram_region;
typedef struct { tp_upgrade_fram_region boot, download, manifests, staging; } tp_upgrade_fram;
tp_result tp_upgrade_fram_init(tp_upgrade_fram *, const tp_i2c_io *);
tp_store_io tp_upgrade_fram_store(tp_upgrade_fram_region *);
#endif
