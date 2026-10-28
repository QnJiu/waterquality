/* In-memory integration backend for real signed upgrades; never a production key. */
#include "tp_secure_upgrade.h"
#include "tp_upgrade_wire.h"
#include <string.h>
#define CAP 1024u
typedef struct { uint8_t slot[2][128]; } store;
static store bs,ds,ms;
static uint8_t images[2][CAP], response[120];
static size_t response_len;
static unsigned erases,writes;
static tp_secure_upgrade secure;
static tp_secure_upgrade_config cfg;
static tp_upgrade_wire wire;
static tp_result rd(void *v,unsigned slot,uint32_t off,void *out,size_t n) {
    if(slot>1 || off>128 || n>128-off) return TP_INVALID;
    memcpy(out,((store *)v)->slot[slot]+off,n); return TP_OK;
}
static tp_result erase(void *v,unsigned slot) {
    if(slot>1) return TP_INVALID; memset(((store *)v)->slot[slot],0xff,128); return TP_OK;
}
static tp_result wr(void *v,unsigned slot,uint32_t off,const void *data,size_t n) {
    if(slot>1 || off>128 || n>128-off) return TP_INVALID;
    uint8_t *dst=((store *)v)->slot[slot]+off; const uint8_t *src=data;
    for(size_t i=0;i<n;i++) if((dst[i]&src[i])!=src[i]) return TP_IO_ERROR;
    for(size_t i=0;i<n;i++) dst[i]&=src[i]; return TP_OK;
}
static tp_result image_read(void *v,uint8_t bank,uint32_t off,void *out,size_t n) {
    (void)v; if(bank>1 || off>CAP || n>CAP-off) return TP_INVALID;
    memcpy(out,images[bank]+off,n); return TP_OK;
}
static tp_result image_erase(void *v,uint8_t bank) {
    (void)v; if(bank>1) return TP_INVALID; memset(images[bank],0xff,CAP); erases++; return TP_OK;
}
static tp_result image_write(void *v,uint8_t bank,uint32_t off,const void *data,size_t n) {
    (void)v; if(bank>1 || off>CAP || n>CAP-off) return TP_INVALID;
    uint8_t *dst=images[bank]+off; const uint8_t *src=data;
    for(size_t i=0;i<n;i++) if((dst[i]&src[i])!=src[i]) return TP_IO_ERROR;
    for(size_t i=0;i<n;i++) dst[i]&=src[i]; writes++; return TP_OK;
}
static tp_result send(void *v,const uint8_t *data,size_t n) {
    (void)v; if(n>sizeof(response)) return TP_INVALID;
    memcpy(response,data,n); response_len=n; return TP_OK;
}
static int setup(void) {
    if(tp_secure_upgrade_init(&secure,&cfg)!=TP_OK) return -1;
    return tp_upgrade_wire_init(&wire,&secure.commands,NULL,send)==TP_OK ? 0 : -1;
}
int sim_secure_init(const uint8_t *pub,const uint8_t *manifest,const uint8_t *image,size_t size) {
    if(!pub || !manifest || !image || size>CAP) return -1;
    memset(&bs,0xff,sizeof(bs)); memset(&ds,0xff,sizeof(ds)); memset(&ms,0xff,sizeof(ms));
    memset(images,0xff,sizeof(images)); memcpy(images[0],image,size); erases=0; writes=0;
    memset(&cfg,0,sizeof(cfg));
    cfg.boot_metadata=(tp_store_io){&bs,rd,erase,wr}; cfg.download_metadata=(tp_store_io){&ds,rd,erase,wr};
    cfg.manifests=(tp_store_io){&ms,rd,erase,wr}; cfg.erase_image=image_erase;
    cfg.read_image=image_read; cfg.write_image=image_write; cfg.capacity[0]=CAP; cfg.capacity[1]=CAP;
    cfg.product_id=0x12345678u; memcpy(cfg.public_key,pub,64);
    if(setup()!=0) return -1;
    tp_download_manifest m;
    if(tp_signed_boot_authenticate(&secure.signed_boot,manifest,116,&m)!=TP_OK || m.bank!=0 || m.version!=1 ||
       tp_signed_boot_download_verify(&secure.signed_boot,&m)!=TP_OK || tp_boot_provision(&secure.boot,0,1)!=TP_OK) return -1;
    return 0;
}
int sim_secure_reboot(void) { return setup(); }
int sim_secure_exchange(const uint8_t *data,size_t n,uint8_t *out,size_t cap) {
    response_len=0;
    for(size_t i=0;i<n;i++) if(tp_upgrade_wire_byte(&wire,data[i])!=TP_OK) return -1;
    if(response_len>cap) return -1; memcpy(out,response,response_len); return (int)response_len;
}
int sim_secure_select(void) { uint8_t bank=99; return tp_boot_select(&secure.boot,&bank)==TP_OK ? (int)bank : -1; }
int sim_secure_confirm(uint8_t bank) { return (int)tp_boot_confirm(&secure.boot,bank); }
int sim_secure_writes(void) { return (int)writes; }
int sim_secure_erases(void) { return (int)erases; }
void sim_secure_corrupt_image(uint8_t bank) { if(bank<2) images[bank][0]^=1; }
void sim_secure_corrupt_manifest(uint8_t bank) { if(bank<2) ms.slot[bank][52]^=1; }
