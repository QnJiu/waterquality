/* Reference board contract: entry from reset, no pre-existing DMA or peripheral
 * activity. I2C2 PB10/PB11 connects only to the upgrade FRAM. */
#include "tp_f407_boot_board.h"
#include "tp_f407_boot_identity.h"
#include "tp_f407_boot_flash.h"
#include "tp_f407_i2c.h"
#include "tp_f407_emergency.h"
#include "stm32f4xx.h"
#include <string.h>
static tp_f407_i2c bus;
static tp_f407_boot_flash flash;
tp_result tp_f407_boot_board_options(tp_f407_boot_options *o) {
    if (!o) return TP_INVALID;
    memset(o,0,sizeof(*o));
    __disable_irq();
    tp_f407_emergency_off();
    uint8_t key_bits=0;
    for (size_t i=0;i<sizeof(o->public_key);i++) key_bits |= tp_f407_boot_public_key[i];
    if (!tp_f407_boot_product_id || !key_bits) return TP_INVALID;
    SystemCoreClockUpdate();
    tp_result r=tp_f407_i2c_init(&bus);
    if (r!=TP_OK) return r;
    tp_f407_boot_flash_reference_init(&flash);
    o->fram_bus=tp_f407_i2c_io(&bus);
    o->image_ctx=&flash;
    o->image_read=tp_f407_boot_flash_read;
    o->product_id=tp_f407_boot_product_id;
    memcpy(o->public_key,tp_f407_boot_public_key,sizeof(o->public_key));
    return TP_OK;
}
tp_result tp_f407_boot_board_quiesce(void) {
    return tp_f407_i2c_poll_shutdown(&bus);
}
