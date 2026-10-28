/* Host integration fixture: no cryptography, persistence or hardware. */
#include "tp_upgrade_wire.h"
#include <string.h>
#define IMAGE_SIZE 768u
typedef struct { uint8_t data[2][128]; } store;
static store boots,downloads;
static uint8_t images[2][IMAGE_SIZE];
static uint8_t response[120];
static size_t response_len;
static unsigned writes;
static tp_upgrade_wire wire;
static tp_result read_store(void *ctx,unsigned slot,uint32_t off,void *out,size_t n) {
    if(slot>1 || off>128 || n>128-off) return TP_INVALID;
    memcpy(out,((store *)ctx)->data[slot]+off,n); return TP_OK;
}
static tp_result erase_store(void *ctx,unsigned slot) {
    if(slot>1) return TP_INVALID;
    memset(((store *)ctx)->data[slot],0xff,128); return TP_OK;
}
static tp_result write_store(void *ctx,unsigned slot,uint32_t off,const void *data,size_t n) {
    if(slot>1 || off>128 || n>128-off) return TP_INVALID;
    uint8_t *p=((store *)ctx)->data[slot]+off; const uint8_t *src=data;
    for(size_t i=0;i<n;i++) if((p[i]&src[i])!=src[i]) return TP_IO_ERROR;
    for(size_t i=0;i<n;i++) p[i]&=src[i];
    return TP_OK;
}
static tp_result erase_image(void *ctx,uint8_t bank) {
    (void)ctx; if(bank>1) return TP_INVALID; memset(images[bank],0xff,IMAGE_SIZE); return TP_OK;
}
static tp_result read_image(void *ctx,uint8_t bank,uint32_t off,void *out,size_t n) {
    (void)ctx; if(bank>1 || off>IMAGE_SIZE || n>IMAGE_SIZE-off) return TP_INVALID;
    memcpy(out,images[bank]+off,n); return TP_OK;
}
static tp_result write_image(void *ctx,uint8_t bank,uint32_t off,const void *data,size_t n) {
    (void)ctx; if(bank>1 || off>IMAGE_SIZE || n>IMAGE_SIZE-off) return TP_INVALID;
    memcpy(images[bank]+off,data,n); writes++; return TP_OK;
}
static bool image_valid(void) {
    for(size_t i=0;i<IMAGE_SIZE;i++) if(images[1][i]!=(uint8_t)i) return false;
    return true;
}
static tp_result verify_boot(void *ctx,uint8_t bank,uint32_t version) {
    (void)ctx;
    if(bank==0 && version==1) return TP_OK;
    return bank==1 && version==2 && image_valid() ? TP_OK : TP_INVALID;
}
static tp_result verify_download(void *ctx,const tp_download_manifest *m) {
    (void)ctx;
    return m->bank==1 && m->version==2 && m->size==IMAGE_SIZE && m->id[0]==1 && image_valid() ? TP_OK : TP_INVALID;
}
static tp_result authenticate(void *ctx,const uint8_t *p,size_t n,tp_download_manifest *m) {
    (void)ctx;
    if(n!=2 || memcmp(p,"ok",2)) return TP_INVALID;
    *m=(tp_download_manifest){{1},2,IMAGE_SIZE,1}; return TP_OK;
}
static tp_result send_response(void *ctx,const uint8_t *p,size_t n) {
    (void)ctx; if(n>sizeof(response)) return TP_INVALID;
    memcpy(response,p,n); response_len=n; return TP_OK;
}
static tp_boot_io boot={{&boots,read_store,erase_store,write_store},NULL,verify_boot};
static tp_download_io download={{&downloads,read_store,erase_store,write_store},NULL,erase_image,read_image,write_image,verify_download};
static tp_upgrade upgrade={&boot,&download};
static tp_upgrade_commands commands={&upgrade,NULL,authenticate};

int sim_init(void) {
    memset(&boots,0xff,sizeof(boots)); memset(&downloads,0xff,sizeof(downloads));
    memset(images,0xff,sizeof(images)); response_len=0; writes=0;
    if(tp_boot_provision(&boot,0,1)!=TP_OK) return -1;
    return tp_upgrade_wire_init(&wire,&commands,NULL,send_response)==TP_OK ? 0 : -1;
}
int sim_exchange(const uint8_t *in,size_t n,uint8_t *out,size_t capacity) {
    response_len=0;
    for(size_t i=0;i<n;i++) if(tp_upgrade_wire_byte(&wire,in[i])!=TP_OK) return -1;
    if(response_len>capacity) return -1;
    memcpy(out,response,response_len); return (int)response_len;
}
int sim_verify_download(void) { return (int)tp_download_finish(&download); }
int sim_writes(void) { return (int)writes; }
int sim_boot_phase(void) {
    tp_boot_state state; return tp_boot_read(&boot,&state)==TP_OK ? (int)state.phase : -1;
}
void sim_reset_transport(void) { tp_upgrade_wire_reset(&wire); }
