#include "tp_f407_boot.h"
#include "tp_factory_provision.h"
#include "boot_signed_fixture.h"
#include <assert.h>
#include <string.h>
typedef struct { uint8_t mem[2048],image[2][64]; unsigned writes,fail_write; } fake;
static tp_result w(void *v,uint8_t dev,const uint8_t *p,size_t n) {
    fake *f=v; assert(dev>=0x50 && dev<=0x57 && n>=2 && (size_t)p[0]+n-1<=256);
    f->writes++; size_t count=n-1;
    if(f->fail_write==f->writes) { memcpy(f->mem+(dev-0x50)*256+p[0],p+1,count/2); return TP_IO_ERROR; }
    memcpy(f->mem+(dev-0x50)*256+p[0],p+1,count); return TP_OK;
}
static tp_result rd(void *v,uint8_t dev,const uint8_t *p,size_t tx,uint8_t *out,size_t n) {
    fake *f=v; assert(dev>=0x50 && dev<=0x57 && tx==1 && (size_t)p[0]+n<=256);
    memcpy(out,f->mem+(dev-0x50)*256+p[0],n); return TP_OK;
}
static tp_result image(void *v,uint8_t bank,uint32_t off,void *p,size_t n) {
    fake *f=v; if(bank>1 || off>64 || n>64-off) return TP_INVALID;
    memcpy(p,f->image[bank]+off,n); return TP_OK;
}
static void init(fake *f,tp_f407_boot *b) {
    memset(f,0,sizeof(*f)); memset(f->mem,0xff,sizeof(f->mem));
    memcpy(f->image[0],test_image0,64); memcpy(f->image[1],test_image1,64);
    tp_f407_boot_options o={.fram_bus={f,w,rd},.image_ctx=f,.image_read=image,.product_id=0x12345678};
    memcpy(o.public_key,test_public,64); assert(tp_f407_boot_init(b,&o)==TP_OK);
}
static tp_result provision(tp_f407_boot *b,const uint8_t *manifest) {
    return tp_factory_provision(&b->signed_boot,&b->target.metadata,manifest,TP_SIGNED_MANIFEST_SIZE);
}
int main(void) {
    fake f; tp_f407_boot b; tp_boot_target target; uint8_t bad[TP_SIGNED_MANIFEST_SIZE];
    init(&f,&b); memcpy(bad,test_manifest0,sizeof(bad)); bad[60]^=1;
    assert(provision(&b,bad)==TP_INVALID && f.writes==0);
    f.image[0][16]^=1; assert(provision(&b,test_manifest0)==TP_INVALID && f.writes==0); f.image[0][16]^=1;
    f.mem[0]=0; assert(provision(&b,test_manifest0)==TP_INVALID && f.writes==0); f.mem[0]=0xff;
    assert(provision(&b,test_manifest0)==TP_OK);
    unsigned writes=f.writes; assert(provision(&b,test_manifest0)==TP_OK && f.writes==writes);
    assert(provision(&b,test_manifest1)==TP_BUSY && f.writes==writes);
    assert(tp_f407_boot_prepare(&b,&target)==TP_OK && target.bank==0);
    unsigned total=writes;
    for(unsigned fail=1;fail<=total;fail++) {
        init(&f,&b); f.fail_write=fail;
        assert(provision(&b,test_manifest0)==TP_IO_ERROR);
        f.fail_write=0;
        tp_result retry=provision(&b,test_manifest0);
        assert(retry==TP_OK || retry==TP_INVALID);
        tp_result boot=tp_f407_boot_prepare(&b,&target);
        if(retry==TP_OK) assert(boot==TP_OK && target.bank==0);
        else assert(boot==TP_INVALID);
    }
    return 0;
}
