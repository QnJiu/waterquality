#include "tp_f030_reference_lock.h"
#include "stm32f0xx.h"
#include <assert.h>
RCC_TypeDef fake_rcc;
GPIO_TypeDef fake_gpio;
int main(void) {
    tp_door_io io=tp_f030_reference_lock_io();
    assert(io.set_unlocked(io.ctx,true)==TP_INVALID);
    tp_f030_reference_lock_emergency_off(); assert(fake_gpio.BSRR==0);
    fake_gpio.MODER=0xffffffffu; fake_gpio.OTYPER=0xffffffffu;
    fake_gpio.PUPDR=0xffffffffu; fake_gpio.OSPEEDR=0xffffffffu;
    assert(tp_f030_reference_lock_init()==TP_OK);
    assert((fake_rcc.AHBENR & RCC_AHBENR_GPIOAEN)!=0);
    assert(fake_gpio.BSRR==(1u<<21));
    assert(((fake_gpio.MODER>>10)&3u)==1 && !(fake_gpio.OTYPER&(1u<<5)));
    assert(!((fake_gpio.PUPDR>>10)&3u) && !((fake_gpio.OSPEEDR>>10)&3u));
    assert(tp_f030_reference_lock_init()==TP_INVALID);
    assert(io.set_unlocked(io.ctx,true)==TP_OK && fake_gpio.BSRR==(1u<<5));
    assert(io.set_unlocked(io.ctx,false)==TP_OK && fake_gpio.BSRR==(1u<<21));
    assert(io.set_unlocked(io.ctx,true)==TP_OK);
    tp_f030_reference_lock_emergency_off(); assert(fake_gpio.BSRR==(1u<<21));
    return 0;
}
