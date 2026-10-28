#include "tp_secure_upgrade.h"
#include <string.h>

static bool store_valid(const tp_store_io *io) {
    return io->read && io->erase && io->write;
}
static tp_result image_read(void *v,uint8_t bank,uint32_t offset,void *out,size_t n) {
    tp_secure_upgrade *u=v;
    return u->config.read_image(u->config.image_ctx,bank,offset,out,n);
}
static tp_result image_erase(void *v,uint8_t bank) {
    tp_secure_upgrade *u=v;
    return u->config.erase_image(u->config.image_ctx,bank);
}
static tp_result image_write(void *v,uint8_t bank,uint32_t offset,const void *data,size_t n) {
    tp_secure_upgrade *u=v;
    return u->config.write_image(u->config.image_ctx,bank,offset,data,n);
}
static tp_result download_verify(void *v,const tp_download_manifest *m) {
    tp_secure_upgrade *u=v;
    return tp_signed_boot_download_verify(&u->signed_boot,m);
}

tp_result tp_secure_upgrade_init(tp_secure_upgrade *u,const tp_secure_upgrade_config *config) {
    if(!u || !config || !store_valid(&config->boot_metadata) ||
       !store_valid(&config->download_metadata) || !store_valid(&config->manifests) ||
       !config->erase_image || !config->read_image || !config->write_image ||
       !config->capacity[0] || !config->capacity[1] || !config->product_id) return TP_INVALID;
    /* Copy before clearing object, allowing a caller's config to reside in it. */
    tp_secure_upgrade_config saved=*config;
    memset(u,0,sizeof(*u)); u->config=saved;
    tp_signed_boot *s=&u->signed_boot;
    memcpy(s->signature.public_key,saved.public_key,sizeof(saved.public_key));
    s->signature.product_id=saved.product_id;
    s->signature.image=(tp_image_hash_io){u,image_read,{saved.capacity[0],saved.capacity[1]}};
    s->manifests=saved.manifests;
    u->boot=(tp_boot_io){saved.boot_metadata,s,tp_signed_boot_verify};
    u->download=(tp_download_io){saved.download_metadata,u,image_erase,image_read,image_write,download_verify};
    u->upgrade=(tp_upgrade){&u->boot,&u->download};
    u->commands=(tp_upgrade_commands){&u->upgrade,s,tp_signed_boot_authenticate};
    return TP_OK;
}
