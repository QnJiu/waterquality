#include "tp_f407_upgrade.h"
#include <string.h>

tp_result tp_f407_upgrade_init(tp_f407_upgrade *u,const tp_f407_upgrade_options *o) {
    if(!u || !o || !o->product_id || !o->fram_bus.write || !o->fram_bus.write_read ||
       !o->flash_ops.read || !o->flash_ops.erase_sector || !o->flash_ops.program_byte)
        return TP_INVALID;
    uint8_t bank;
    if(o->running_vector==TP_F407_IMAGE0_BASE) bank=0;
    else if(o->running_vector==TP_F407_IMAGE1_BASE) bank=1;
    else return TP_INVALID;
    tp_f407_upgrade_options options=*o;
    memset(u,0,sizeof(*u));
    tp_result r=tp_upgrade_fram_init(&u->fram,&options.fram_bus);
    if(r!=TP_OK) return r;
    r=tp_f407_image_flash_init(&u->image,&options.flash_ops,bank);
    if(r!=TP_OK) return r;
    tp_secure_upgrade_config config={
        .boot_metadata=tp_upgrade_fram_store(&u->fram.boot),
        .download_metadata=tp_upgrade_fram_store(&u->fram.download),
        .manifests=tp_upgrade_fram_store(&u->fram.manifests),
        .image_ctx=&u->image,.erase_image=tp_f407_image_erase,
        .read_image=tp_f407_image_read,.write_image=tp_f407_image_write,
        .capacity={TP_F407_IMAGE_BANK_SIZE,TP_F407_IMAGE_BANK_SIZE},
        .product_id=options.product_id
    };
    memcpy(config.public_key,options.public_key,sizeof(config.public_key));
    r=tp_secure_upgrade_init(&u->secure,&config);
    if(r!=TP_OK) return r;
    tp_boot_state state;
    r=tp_boot_read(&u->secure.boot,&state);
    if(r!=TP_OK) return r;
    if((state.phase==TP_BOOT_TRIAL ? state.candidate_bank : state.active_bank)!=bank)
        return TP_INVALID;
    u->running_bank=bank;
    return TP_OK;
}
