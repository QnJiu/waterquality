#include "tp_f407_handoff.h"
#include "tp_f407_image_flash.h"
#include "stm32f4xx.h"
#ifdef TP_F407_HANDOFF_TEST
extern void tp_handoff_transfer(uint32_t,uint32_t);
#else
/* Do not touch the old C stack after writing CONTROL/MSP. IRQ sources are
 * disabled and pending IRQs cleared before unmasking at the new stack. */
__attribute__((naked,noreturn)) static void tp_handoff_transfer(uint32_t stack __attribute__((unused)),uint32_t entry __attribute__((unused))) {
    __asm volatile("movs r2, #0\n"
                   "msr basepri, r2\n"
                   "msr msp, r0\n"
                   "msr control, r2\n"
                   "isb\n"
                   "cpsie i\n"
                   "bx r1\n");
}
#endif
tp_result tp_f407_handoff(const tp_boot_target *t) {
    if(!t || t->bank>1 || __get_IPSR()!=0 || (__get_CONTROL()&1u) || __get_FAULTMASK()!=0) return TP_INVALID;
    uint32_t base=t->bank ? TP_F407_IMAGE1_BASE : TP_F407_IMAGE0_BASE;
    uint32_t address=t->entry&~1u;
    if(t->vector!=base || (t->stack&7u) || t->stack<=0x20000000u || t->stack>0x20020000u ||
       !(t->entry&1u) || address<base || address-base>TP_F407_IMAGE_BANK_SIZE-2u) return TP_INVALID;
    __disable_irq();
    SysTick->CTRL=0; SysTick->LOAD=0; SysTick->VAL=0;
    /* F407 has 82 external interrupts; write-one-to-clear registers. */
    for(unsigned i=0;i<3;i++) { NVIC->ICER[i]=0xffffffffu; NVIC->ICPR[i]=0xffffffffu; }
    SCB->ICSR=SCB_ICSR_PENDSTCLR_Msk|SCB_ICSR_PENDSVCLR_Msk;
    SCB->VTOR=t->vector;
    __DSB(); __ISB();
    tp_handoff_transfer(t->stack,t->entry);
#ifdef TP_F407_HANDOFF_TEST
    return TP_OK;
#endif
}
