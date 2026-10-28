#define main signed_boot_original_main
#include "test_signed_boot.c"
#undef main
#include "tp_staged_upgrade.h"
static const char *signed_v1="5450553178563412020000000300000001000000ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015adad628c012e2dd70b93c667ef7166ff7dbf199f87e4105fbf15f89ba483f0e7f169b5287eedeef89d4a6299c0fc066a9a94b7418f92722a08f81f22d5b9f12679";
typedef struct { fake target; tp_signed_boot boot; uint8_t source[3]; unsigned erases; } fixture;
static tp_result source_read(void *v,uint8_t bank,uint32_t off,void *out,size_t n) {
    fixture *f=v; if(bank!=1 || off+n>3) return TP_INVALID;
    memcpy(out,f->source+off,n); return TP_OK;
}
static tp_result dest_read(void *v,uint8_t bank,uint32_t off,void *out,size_t n) {
    fixture *f=v; return read_image(&f->target,bank,off,out,n);
}
static tp_result dest_erase(void *v,uint8_t bank) {
    fixture *f=v; if(bank!=1) return TP_INVALID;
    f->erases++; memset(f->target.image[bank],0xff,3); return TP_OK;
}
static tp_result dest_write(void *v,uint8_t bank,uint32_t off,const void *in,size_t n) {
    fixture *f=v; if(bank!=1 || off+n>3) return TP_INVALID;
    memcpy(f->target.image[bank]+off,in,n); return TP_OK;
}
#include "tp_stage_receiver.h"
#include "tp_stage_session.h"
typedef struct { bool locked, deny, safe; unsigned unlocks; } gate;
static bool take_lock(void *v) { gate *g=v; if(g->deny || g->locked) return false; g->locked=true; return true; }
static void release_lock(void *v) { gate *g=v; assert(g->locked); g->locked=false; g->unlocks++; }
static tp_result make_safe(void *v) { return ((gate *)v)->safe ? TP_OK : TP_BUSY; }
#include "tp_stage_commands.h"
#include "tp_upgrade_stream.h"
static uint8_t response[TP_UP_WIRE_ENCODED_MAX];
static size_t response_len;
static tp_result capture(void *ctx,const uint8_t *data,size_t n) {
    (void)ctx; assert(n<=sizeof(response)); memcpy(response,data,n); response_len=n; return TP_OK;
}
static void send_command(tp_upgrade_stream *stream,uint8_t cmd,const uint8_t *payload,size_t n) {
    uint8_t frame[TP_UP_WIRE_ENCODED_MAX]; size_t length;
    assert(tp_upgrade_wire_request(cmd,42,payload,n,frame,sizeof(frame),&length)==TP_OK);
    response_len=0;
    for(size_t i=0;i<length;i++) assert(tp_upgrade_stream_byte(stream,frame[i],1)==TP_OK);
    assert(response_len>10 && response[0]==0x7e && response[response_len-1]==0x7e);
    assert(response[7]==TP_OK);
}
static tp_result stage_erase(void *v,uint8_t bank) {
    fixture *f=v; if(bank!=1) return TP_INVALID;
    f->erases++; memset(f->source,0xff,3); return TP_OK;
}
static tp_result stage_write(void *v,uint8_t bank,uint32_t off,const void *p,size_t n) {
    fixture *f=v; if(bank!=1 || off+n>3) return TP_INVALID;
    memcpy(f->source+off,p,n); return TP_OK;
}
#include "tp_freertos_stage_transport.h"
typedef struct { uint8_t in[2048]; size_t len,pos; bool disconnect,fail_send; unsigned sent,finished; tp_result result; } link;
static tp_result waiting_for_maintenance(void *v) { return ((gate *)v)->safe ? TP_OK : TP_LOCK_BUSY; }
static tp_result receive_byte(void *v,uint8_t *b) {
    link *l=v; if(l->disconnect) return TP_IO_ERROR;
    if(l->pos==l->len) return TP_BUSY; *b=l->in[l->pos++]; return TP_OK;
}
static tp_result send_bytes(void *v,const uint8_t *b,size_t n) {
    link *l=v; (void)b; assert(n>0); l->sent++; return l->fail_send ? TP_IO_ERROR : TP_OK;
}
static void done(void *v,tp_result r) { link *l=v; l->finished++; l->result=r; }
BaseType_t xTaskGetSchedulerState(void) { return 0; }
TickType_t xTaskGetTickCount(void) { return 0; }
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,uint16_t stack,void *arg,UBaseType_t prio,TaskHandle_t *task) {
    (void)fn; (void)name; (void)stack; (void)arg; (void)prio; *task=(void *)1; return pdPASS;
}
void vTaskDelay(TickType_t ticks) { (void)ticks; }
static void request(link *l,uint8_t cmd,const uint8_t *p,size_t n) {
    assert(l->pos==l->len); l->pos=l->len=0;
    assert(tp_upgrade_wire_request(cmd,1,p,n,l->in,sizeof(l->in),&l->len)==TP_OK);
}
static void drain(tp_freertos_stage_transport *t,link *l,uint32_t now) {
    while(l->pos<l->len) tp_freertos_stage_transport_step(t,now);
}
int main(void) {
    fixture f; init(&f.target,&f.boot); f.erases=0;
    fake boot_store,stage_meta; tp_signed_boot unused; init(&boot_store,&unused); init(&stage_meta,&unused);
    tp_secure_upgrade_config c={0};
    c.boot_metadata=(tp_store_io){&boot_store,read_meta,erase_meta,write_meta};
    c.download_metadata=(tp_store_io){&f.target,read_meta,erase_meta,write_meta};
    c.manifests=(tp_store_io){&f.target,read_manifest,erase_manifest,write_manifest};
    c.image_ctx=&f; c.erase_image=dest_erase; c.read_image=dest_read; c.write_image=dest_write;
    c.capacity[0]=c.capacity[1]=3; c.product_id=f.boot.signature.product_id;
    memcpy(c.public_key,f.boot.signature.public_key,64);
    tp_secure_upgrade u; assert(tp_secure_upgrade_init(&u,&c)==TP_OK);
    const char *v0="5450553178563412010000000300000000000000ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad951584689bde05b6bcb46ada8a291c99c8636798711a92c0b11b3da52d664e42a40bb63cca6e220774961b8f1eb8b1a1b71ec31c225c163e8a3c24e8003fb2d7";
    uint8_t b0[116],b1[116]; hex(v0,b0,116); hex(signed_v1,b1,116);
    tp_download_manifest m; assert(tp_signed_boot_authenticate(&u.signed_boot,b0,116,&m)==TP_OK);
    assert(tp_signed_boot_download_verify(&u.signed_boot,&m)==TP_OK);
    assert(tp_boot_provision(&u.boot,0,1)==TP_OK);
    tp_download_io storage={{&stage_meta,read_meta,erase_meta,write_meta},&f,stage_erase,source_read,stage_write,NULL};
    uint32_t capacities[2]={3,3}; gate g={0}; tp_stage_session s;
    assert(tp_stage_session_init(&s,&u,&storage,capacities,&g,take_lock,release_lock,waiting_for_maintenance)==TP_OK);
    link l={0}; tp_stage_transport_io io={&l,receive_byte,send_bytes,done}; tp_freertos_stage_transport t;
    assert(tp_freertos_stage_transport_start(&t,&s,&io,20,100,1,1024,1)==TP_OK);
    request(&l,TP_UP_CMD_BEGIN,b1,116); drain(&t,&l,0xfffffff0u); assert(g.locked && l.sent==1);
    tp_freertos_stage_transport_step(&t,84); assert(!g.locked); /* wraparound timeout */
    request(&l,TP_UP_CMD_BEGIN,b1,116); drain(&t,&l,100); assert(g.locked && f.erases==1);
    l.disconnect=true; tp_freertos_stage_transport_step(&t,101); assert(!g.locked); l.disconnect=false;
    request(&l,TP_UP_CMD_BEGIN,b1,116); drain(&t,&l,110); assert(g.locked && f.erases==1);
    uint8_t data[]={0,0,0,0,'a','b','c'}; request(&l,TP_UP_CMD_WRITE,data,sizeof(data)); drain(&t,&l,111);
    l.fail_send=true; request(&l,TP_UP_CMD_FINISH,NULL,0); drain(&t,&l,112);
    assert(!g.locked && !t.pending && !l.finished); l.fail_send=false;
    request(&l,TP_UP_CMD_BEGIN,b1,116); drain(&t,&l,113);
    request(&l,TP_UP_CMD_FINISH,NULL,0); drain(&t,&l,114); assert(t.pending && g.locked);
    tp_freertos_stage_transport_step(&t,115); assert(!t.installing && g.locked);
    tp_freertos_stage_transport_step(&t,214);
    assert(!g.locked && l.finished==1 && l.result==TP_TIMEOUT);
    request(&l,TP_UP_CMD_BEGIN,b1,116); drain(&t,&l,215);
    request(&l,TP_UP_CMD_FINISH,NULL,0); drain(&t,&l,216);
    g.safe=true; tp_freertos_stage_transport_step(&t,217); assert(t.installing);
    for(unsigned i=218; i<226 && l.finished<2; i++) tp_freertos_stage_transport_step(&t,i);
    assert(l.finished==2 && l.result==TP_OK && !g.locked);
    tp_boot_state state; assert(tp_boot_read(&u.boot,&state)==TP_OK && state.phase==TP_BOOT_READY);
    return 0;
}
