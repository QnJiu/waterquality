#include "tp_f407_upgrade.h"
#include <assert.h>
#include <string.h>
typedef struct { uint8_t mem[2048]; bool error; unsigned erases,programs; } fake;
static tp_result bus_write(void *ctx,uint8_t dev,const uint8_t *p,size_t n) {
    fake *f=ctx; if(f->error) return TP_IO_ERROR;
    assert(dev>=0x50 && dev<=0x57 && n>=2 && (size_t)p[0]+n-1<=256);
    memcpy(f->mem+(dev-0x50)*256+p[0],p+1,n-1); return TP_OK;
}
static tp_result bus_read(void *ctx,uint8_t dev,const uint8_t *p,size_t n,uint8_t *out,size_t len) {
    fake *f=ctx; if(f->error) return TP_IO_ERROR;
    assert(dev>=0x50 && dev<=0x57 && n==1 && (size_t)p[0]+len<=256);
    memcpy(out,f->mem+(dev-0x50)*256+p[0],len); return TP_OK;
}
static tp_result flash_read(void *ctx,uint32_t address,void *out,size_t n) { (void)ctx; (void)address; memset(out,0xff,n); return TP_OK; }
static tp_result flash_erase(void *ctx,unsigned sector) { fake *f=ctx; assert(sector>=5 && sector<=10); f->erases++; return TP_OK; }
static tp_result flash_write(void *ctx,uint32_t address,uint8_t b) { fake *f=ctx; (void)address; (void)b; f->programs++; return TP_OK; }
static tp_result verify(void *ctx,uint8_t bank,uint32_t version) { (void)ctx; return (bank==0 && version==1)||(bank==1 && version==2) ? TP_OK : TP_INVALID; }
int main(void) {
    fake f={0}; memset(f.mem,0xff,sizeof(f.mem));
    tp_i2c_io bus={&f,bus_write,bus_read}; tp_upgrade_fram fram;
    assert(tp_upgrade_fram_init(&fram,&bus)==TP_OK);
    tp_boot_io boot={tp_upgrade_fram_store(&fram.boot),NULL,verify};
    tp_f407_upgrade_options options={.fram_bus=bus,.flash_ops={&f,flash_read,flash_erase,flash_write},
        .running_vector=TP_F407_IMAGE0_BASE,.product_id=123};
    tp_f407_upgrade u; assert(tp_f407_upgrade_init(&u,&options)==TP_INVALID); /* blank */
    assert(tp_boot_provision(&boot,0,1)==TP_OK);
    assert(tp_f407_upgrade_init(&u,&options)==TP_OK && u.running_bank==0);
    assert(tp_f407_image_erase(&u.image,0)==TP_INVALID && f.erases==0);
    assert(tp_f407_image_erase(&u.image,1)==TP_OK && f.erases==3);
    options.running_vector=TP_F407_IMAGE0_BASE+512; assert(tp_f407_upgrade_init(&u,&options)==TP_INVALID);
    options.running_vector=TP_F407_IMAGE1_BASE; assert(tp_f407_upgrade_init(&u,&options)==TP_INVALID);
    assert(tp_boot_stage(&boot,1,2)==TP_OK);
    options.running_vector=TP_F407_IMAGE0_BASE; assert(tp_f407_upgrade_init(&u,&options)==TP_OK);
    uint8_t bank; assert(tp_boot_select(&boot,&bank)==TP_OK && bank==1);
    assert(tp_f407_upgrade_init(&u,&options)==TP_INVALID);
    options.running_vector=TP_F407_IMAGE1_BASE; assert(tp_f407_upgrade_init(&u,&options)==TP_OK && u.running_bank==1);
    assert(tp_f407_image_erase(&u.image,1)==TP_INVALID && f.erases==3);
    f.error=true; assert(tp_f407_upgrade_init(&u,&options)==TP_IO_ERROR);
    return 0;
}
