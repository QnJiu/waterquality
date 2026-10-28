#include "tp_f407_boot.h"
#include "tp_trial_confirm.h"
#include "boot_signed_fixture.h"
#include <assert.h>
#include <string.h>
typedef struct { uint8_t mem[2048],image[2][64]; bool bus_error,image_error; } fake;
static tp_result write_bus(void *v,uint8_t dev,const uint8_t *p,size_t n) {
    fake *f=v; if(f->bus_error) return TP_IO_ERROR;
    assert(dev>=0x50 && dev<=0x57 && n>=2 && (size_t)p[0]+n-1<=256);
    memcpy(f->mem+(dev-0x50)*256+p[0],p+1,n-1); return TP_OK;
}
static tp_result read_bus(void *v,uint8_t dev,const uint8_t *p,size_t tx,uint8_t *out,size_t n) {
    fake *f=v; if(f->bus_error) return TP_IO_ERROR;
    assert(dev>=0x50 && dev<=0x57 && tx==1 && (size_t)p[0]+n<=256);
    memcpy(out,f->mem+(dev-0x50)*256+p[0],n); return TP_OK;
}
static tp_result read_image(void *v,uint8_t bank,uint32_t off,void *p,size_t n) {
    fake *f=v; if(f->image_error) return TP_IO_ERROR;
    if(bank>1 || off>64 || n>64-off) return TP_INVALID;
    memcpy(p,f->image[bank]+off,n); return TP_OK;
}
int main(void) {
    fake f={0}; memset(f.mem,0xff,sizeof(f.mem)); memcpy(f.image[0],test_image0,64); memcpy(f.image[1],test_image1,64);
    tp_f407_boot_options o={.fram_bus={&f,write_bus,read_bus},.image_ctx=&f,.image_read=read_image,.product_id=0x12345678}; memcpy(o.public_key,test_public,64);
    tp_f407_boot b; tp_boot_target target={.bank=9}; tp_download_manifest m;
    assert(tp_f407_boot_init(&b,&o)==TP_OK);
    assert(tp_f407_boot_prepare(&b,&target)==TP_INVALID && target.bank==9); /* blank metadata */
    assert(tp_signed_boot_authenticate(&b.signed_boot,test_manifest0,sizeof(test_manifest0),&m)==TP_OK);
    assert(tp_signed_boot_download_verify(&b.signed_boot,&m)==TP_OK);
    tp_boot_io boot={b.target.metadata,&b.signed_boot,tp_signed_boot_verify};
    assert(tp_boot_provision(&boot,0,1)==TP_OK);
    assert(tp_signed_boot_authenticate(&b.signed_boot,test_manifest1,sizeof(test_manifest1),&m)==TP_OK);
    assert(tp_signed_boot_download_verify(&b.signed_boot,&m)==TP_OK);
    assert(tp_boot_stage(&boot,1,2)==TP_OK);
    /* Fresh assembly does not depend on cached download manifest. */
    assert(tp_f407_boot_init(&b,&o)==TP_OK);
    assert(tp_f407_boot_prepare(&b,&target)==TP_OK && target.bank==1 && target.vector==TP_F407_IMAGE1_BASE);
    assert(tp_f407_boot_prepare(&b,&target)==TP_OK && target.bank==0); /* unconfirmed trial */
    assert(tp_boot_stage(&boot,1,2)==TP_OK); f.image[1][15]^=1;
    assert(tp_f407_boot_prepare(&b,&target)==TP_OK && target.bank==0); /* candidate hash failure */
    f.image[0][15]^=1; target.bank=9;
    assert(tp_f407_boot_prepare(&b,&target)==TP_INVALID && target.bank==9);
    f.image[0][15]^=1; f.image[1][15]^=1;
    assert(tp_boot_stage(&boot,1,2)==TP_OK);
    assert(tp_f407_boot_prepare(&b,&target)==TP_OK && target.bank==1);
    tp_trial_confirm gate;
    assert(tp_trial_confirm_init(&gate,&boot,1,100,50)==TP_OK);
    assert(tp_trial_confirm_poll(&gate,0,true)==TP_BUSY);
    assert(tp_trial_confirm_poll(&gate,50,true)==TP_BUSY);
    assert(tp_trial_confirm_poll(&gate,100,true)==TP_OK);
    assert(tp_f407_boot_prepare(&b,&target)==TP_OK && target.bank==1);
    f.bus_error=true;
    assert(tp_f407_boot_prepare(&b,&target)==TP_IO_ERROR); f.bus_error=false; f.image_error=true;
    assert(tp_f407_boot_prepare(&b,&target)==TP_IO_ERROR);
    o.product_id=0; assert(tp_f407_boot_init(&b,&o)==TP_INVALID);
    return 0;
}
