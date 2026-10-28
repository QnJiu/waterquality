#include "tp_boot_target.h"
#include <assert.h>
#include <string.h>
typedef struct { uint8_t metadata[2][64], image[2][64]; uint32_t size[2]; bool fail; } fake;
static tp_result rdmeta(void *v,unsigned s,uint32_t o,void *p,size_t n) { fake *f=v; assert(s<2 && o+n<=64); memcpy(p,f->metadata[s]+o,n); return TP_OK; }
static tp_result wrmeta(void *v,unsigned s,uint32_t o,const void *p,size_t n) { fake *f=v; assert(s<2 && o+n<=64); memcpy(f->metadata[s]+o,p,n); return TP_OK; }
static tp_result erase(void *v,unsigned s) { fake *f=v; assert(s<2); memset(f->metadata[s],0xff,64); return TP_OK; }
static tp_result image(void *v,uint8_t b,uint32_t o,void *p,size_t n) { fake *f=v; if(f->fail) return TP_IO_ERROR; assert(b<2 && o+n<=64); memcpy(p,f->image[b]+o,n); return TP_OK; }
static tp_result check(void *v,uint8_t b,uint32_t ver,uint32_t *size) { fake *f=v; if(f->fail) return TP_IO_ERROR; if(b>1 || ver!=b+1u) return TP_INVALID; *size=f->size[b]; return TP_OK; }
static tp_result check_boot(void *v,uint8_t b,uint32_t ver) { uint32_t size; return check(v,b,ver,&size); }
static void put(uint8_t *p,uint32_t n) { for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(n>>(i*8)); }
int main(void) {
    fake f={0}; memset(f.metadata,0xff,sizeof(f.metadata)); f.size[0]=f.size[1]=64;
    put(f.image[0],0x20020000); put(f.image[0]+4,0x08020021); put(f.image[1],0x10010000); put(f.image[1]+4,0x08080021);
    tp_store_io store={&f,rdmeta,erase,wrmeta}; tp_boot_io boot={store,&f,check_boot};
    tp_boot_target_config cfg={store,&f,&f,check,image,{0x08020000,0x08080000},{64,64},{0x20000000,0x10000000},{0x20020000,0x10010000},512};
    tp_boot_target t={.bank=9}; tp_boot_state state;
    assert(tp_boot_provision(&boot,0,1)==TP_OK);
    assert(tp_boot_target_prepare(&cfg,&t)==TP_OK && t.bank==0 && t.stack==0x20020000 && t.entry==0x08020021);
    assert(tp_boot_stage(&boot,1,2)==TP_OK);
    assert(tp_boot_target_prepare(&cfg,&t)==TP_OK && t.bank==1 && t.vector==0x08080000);
    assert(tp_boot_read(&boot,&state)==TP_OK && state.phase==TP_BOOT_TRIAL);
    assert(tp_boot_target_prepare(&cfg,&t)==TP_OK && t.bank==0); /* unconfirmed reset rolls back */
    assert(tp_boot_stage(&boot,1,2)==TP_OK); put(f.image[1]+4,0x08080040|1u);
    assert(tp_boot_target_prepare(&cfg,&t)==TP_OK && t.bank==0); /* candidate entry outside signed bytes */
    assert(tp_boot_read(&boot,&state)==TP_OK && state.phase==TP_BOOT_IDLE);
    uint32_t badsp[]={0,0x20000000,0x20020004,0x20020008};
    for(unsigned i=0;i<sizeof(badsp)/sizeof(badsp[0]);i++) { put(f.image[0],badsp[i]); t.bank=9; assert(tp_boot_target_prepare(&cfg,&t)==TP_INVALID && t.bank==9); }
    put(f.image[0],0x20020000);
    uint32_t badentry[]={0x08020020,0x0801ffff,0x08020041,0xffffffff};
    for(unsigned i=0;i<sizeof(badentry)/sizeof(badentry[0]);i++) { put(f.image[0]+4,badentry[i]); assert(tp_boot_target_prepare(&cfg,&t)==TP_INVALID); }
    put(f.image[0]+4,0x08020021); f.size[0]=7; assert(tp_boot_target_prepare(&cfg,&t)==TP_INVALID); f.size[0]=65; assert(tp_boot_target_prepare(&cfg,&t)==TP_INVALID); f.size[0]=64;
    f.fail=true; assert(tp_boot_target_prepare(&cfg,&t)==TP_IO_ERROR); f.fail=false;
    cfg.base[1]=cfg.base[0]; assert(tp_boot_target_prepare(&cfg,&t)==TP_INVALID);
    cfg.base[1]=0x08080001; assert(tp_boot_target_prepare(&cfg,&t)==TP_INVALID);
    return 0;
}
