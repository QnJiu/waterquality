#include "tp_f407_boot_flash.h"
#include <assert.h>
#include <string.h>
static uint8_t a[TP_F407_IMAGE_BANK_SIZE],b[TP_F407_IMAGE_BANK_SIZE];
int main(void) {
    tp_f407_boot_flash flash={{a,b}}, reference;
    uint8_t out[4]={0};
    a[0]=0x12; a[1]=0x34; b[TP_F407_IMAGE_BANK_SIZE-1]=0x56;
    assert(tp_f407_boot_flash_read(&flash,0,0,out,2)==TP_OK && out[0]==0x12 && out[1]==0x34);
    assert(tp_f407_boot_flash_read(&flash,1,TP_F407_IMAGE_BANK_SIZE-1,out,1)==TP_OK && out[0]==0x56);
    assert(tp_f407_boot_flash_read(&flash,1,TP_F407_IMAGE_BANK_SIZE,NULL,0)==TP_OK);
    assert(tp_f407_boot_flash_read(&flash,1,TP_F407_IMAGE_BANK_SIZE,out,1)==TP_INVALID);
    assert(tp_f407_boot_flash_read(&flash,1,UINT32_MAX,out,2)==TP_INVALID);
    assert(tp_f407_boot_flash_read(&flash,2,0,out,1)==TP_INVALID);
    assert(tp_f407_boot_flash_read(&flash,0,0,NULL,1)==TP_INVALID);
    assert(tp_f407_boot_flash_read(NULL,0,0,out,1)==TP_INVALID);
    flash.slot[0]=NULL; assert(tp_f407_boot_flash_read(&flash,0,0,out,1)==TP_INVALID);
    tp_f407_boot_flash_reference_init(&reference);
    assert((uintptr_t)reference.slot[0]==TP_F407_IMAGE0_BASE && (uintptr_t)reference.slot[1]==TP_F407_IMAGE1_BASE);
    return 0;
}
