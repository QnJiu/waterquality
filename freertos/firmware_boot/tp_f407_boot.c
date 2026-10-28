#include "tp_f407_boot.h"
#include "tp_factory_provision.h"
#include <string.h>
tp_result tp_f407_boot_init(tp_f407_boot *b,const tp_f407_boot_options *o) {
    if(!b || !o || !o->fram_bus.write || !o->fram_bus.write_read || !o->image_read || !o->product_id) return TP_INVALID;
    memset(b,0,sizeof(*b)); tp_result r=tp_upgrade_fram_init(&b->fram,&o->fram_bus); if(r!=TP_OK) return r;
    memcpy(b->signed_boot.signature.public_key,o->public_key,64);
    b->signed_boot.signature.product_id=o->product_id;
    b->signed_boot.signature.image=(tp_image_hash_io){o->image_ctx,o->image_read,{TP_F407_IMAGE_BANK_SIZE,TP_F407_IMAGE_BANK_SIZE}};
    b->signed_boot.manifests=tp_upgrade_fram_store(&b->fram.manifests);
    b->target=(tp_boot_target_config){
        .metadata=tp_upgrade_fram_store(&b->fram.boot), .verify_ctx=&b->signed_boot, .read_ctx=o->image_ctx,
        .verify=tp_signed_boot_verify_size, .read=o->image_read,
        .base={TP_F407_IMAGE0_BASE,TP_F407_IMAGE1_BASE}, .capacity={TP_F407_IMAGE_BANK_SIZE,TP_F407_IMAGE_BANK_SIZE},
        .ram_start={0x20000000u,0}, .ram_end={0x20020000u,0}, .vector_alignment=512};
    return TP_OK;
}
tp_result tp_f407_boot_prepare(tp_f407_boot *b,tp_boot_target *target) {
    if(!b || !target) return TP_INVALID;
    return tp_boot_target_prepare(&b->target,target);
}

tp_result tp_f407_boot_factory_provision(tp_f407_boot *b,const uint8_t *manifest,size_t size) {
    if (!b) return TP_INVALID;
    tp_download_manifest m;
    tp_result r=tp_signed_boot_authenticate(&b->signed_boot,manifest,size,&m);
    if(r!=TP_OK) return r;
    tp_boot_target target;
    r=tp_boot_target_check_vector(&b->target,m.bank,m.size,&target);
    if(r!=TP_OK) return r;
    return tp_factory_provision(&b->signed_boot,&b->target.metadata,manifest,size);
}
