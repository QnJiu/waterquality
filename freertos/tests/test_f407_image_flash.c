#include "tp_f407_image_flash.h"
#include <assert.h>
#include <string.h>
static uint8_t memory[2*TP_F407_IMAGE_BANK_SIZE];
static unsigned erases[12], programs, fail_at;
static tp_result rd(void *ctx,uint32_t address,void *out,size_t n) {
    (void)ctx; assert(address>=TP_F407_IMAGE0_BASE && address-TP_F407_IMAGE0_BASE<=sizeof(memory));
    size_t offset=address-TP_F407_IMAGE0_BASE; assert(n<=sizeof(memory)-offset);
    memcpy(out,memory+offset,n); return TP_OK;
}
static tp_result erase(void *ctx,unsigned sector) {
    (void)ctx; assert(sector>=5 && sector<=10); erases[sector]++;
    memset(memory+(sector-5)*128u*1024u,0xff,128u*1024u); return TP_OK;
}
static tp_result program(void *ctx,uint32_t address,uint8_t value) {
    (void)ctx; assert(address>=TP_F407_IMAGE0_BASE && address<TP_F407_IMAGE0_BASE+sizeof(memory));
    programs++; if(programs==fail_at) return TP_IO_ERROR;
    uint8_t *p=&memory[address-TP_F407_IMAGE0_BASE]; assert((*p&value)==value); *p&=value; return TP_OK;
}
int main(void) {
    tp_f407_flash_ops ops={NULL,rd,erase,program}; tp_f407_image_flash f;
    memset(memory,0,sizeof(memory)); assert(tp_f407_image_flash_init(&f,&ops,0)==TP_OK);
    assert(tp_f407_image_erase(&f,0)==TP_INVALID);
    assert(tp_f407_image_erase(&f,1)==TP_OK);
    for(unsigned i=0;i<12;i++) assert(erases[i]==(i>=8 && i<=10));
    uint8_t payload[]={0x12,0x34,0x56,0x78},readback[4];
    assert(tp_f407_image_write(&f,0,0,payload,4)==TP_INVALID);
    assert(tp_f407_image_write(&f,1,TP_F407_IMAGE_BANK_SIZE-3,payload,4)==TP_INVALID);
    assert(tp_f407_image_read(&f,1,UINT32_MAX,readback,4)==TP_INVALID);
    fail_at=3; assert(tp_f407_image_write(&f,1,0,payload,4)==TP_IO_ERROR);
    fail_at=0; assert(tp_f407_image_write(&f,1,0,payload,4)==TP_OK);
    assert(tp_f407_image_read(&f,1,0,readback,4)==TP_OK && !memcmp(readback,payload,4));
    unsigned before=programs; assert(tp_f407_image_write(&f,1,0,payload,4)==TP_OK && programs==before);
    uint8_t invalid[]={0xff,0,0,0}; assert(tp_f407_image_write(&f,1,0,invalid,4)==TP_INVALID && programs==before);
    assert(tp_f407_image_flash_set_active(&f,1)==TP_OK);
    assert(tp_f407_image_erase(&f,1)==TP_INVALID && tp_f407_image_erase(&f,0)==TP_OK);
    for(unsigned i=5;i<=7;i++) assert(erases[i]==1);
    return 0;
}
