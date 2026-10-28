#include "tp_f407_handoff.h"
#include "tp_f407_image_flash.h"
#include "stm32f4xx.h"
#include <assert.h>
#include <string.h>
fake_systick test_systick; fake_nvic test_nvic; fake_scb test_scb;
uint32_t test_ipsr,test_control,test_faultmask,test_irq_disabled,test_dsb,test_isb;
static unsigned calls;
void tp_handoff_transfer(uint32_t stack,uint32_t entry) {
    assert(test_irq_disabled==1 && test_dsb==1 && test_isb==1);
    assert(!test_systick.CTRL && !test_systick.LOAD && !test_systick.VAL);
    for(unsigned i=0;i<3;i++) assert(test_nvic.ICER[i]==0xffffffffu && test_nvic.ICPR[i]==0xffffffffu);
    assert(test_scb.ICSR==(SCB_ICSR_PENDSTCLR_Msk|SCB_ICSR_PENDSVCLR_Msk));
    assert(test_scb.VTOR==TP_F407_IMAGE1_BASE && stack==0x20020000 && entry==TP_F407_IMAGE1_BASE+0x21); calls++;
}
int main(void) {
    tp_boot_target t={1,TP_F407_IMAGE1_BASE,0x20020000,TP_F407_IMAGE1_BASE+0x21};
    assert(tp_f407_handoff(NULL)==TP_INVALID);
    test_ipsr=1; assert(tp_f407_handoff(&t)==TP_INVALID); test_ipsr=0;
    test_control=1; assert(tp_f407_handoff(&t)==TP_INVALID); test_control=0;
    test_faultmask=1; assert(tp_f407_handoff(&t)==TP_INVALID); test_faultmask=0;
    t.vector++; assert(tp_f407_handoff(&t)==TP_INVALID); t.vector--;
    t.stack=0x20000000; assert(tp_f407_handoff(&t)==TP_INVALID);
    t.stack=0x20020004; assert(tp_f407_handoff(&t)==TP_INVALID);
    t.stack=0x20020000; t.entry=TP_F407_IMAGE0_BASE+1; assert(tp_f407_handoff(&t)==TP_INVALID);
    t.entry=TP_F407_IMAGE1_BASE+0x20; assert(tp_f407_handoff(&t)==TP_INVALID);
    t.entry=TP_F407_IMAGE1_BASE+TP_F407_IMAGE_BANK_SIZE+1; assert(tp_f407_handoff(&t)==TP_INVALID);
    assert(!calls && !test_irq_disabled && !test_dsb && !test_isb);
    t.entry=TP_F407_IMAGE1_BASE+0x21; memset(&test_systick,0xff,sizeof(test_systick));
    assert(tp_f407_handoff(&t)==TP_OK && calls==1); return 0;
}
