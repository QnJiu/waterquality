#include "tp_f407_status_input.h"
#include "stm32f4xx.h"
#include <assert.h>
RCC_TypeDef fake_rcc; GPIO_TypeDef fake_gpio;
int main(void) {
    fake_gpio.MODER=0xffffffffu; fake_gpio.PUPDR=0xffffffffu;
    tp_f407_status_input_init();
    assert(fake_rcc.AHB1ENR & RCC_AHB1ENR_GPIOCEN);
    assert(((fake_gpio.MODER>>24)&3u)==0 && ((fake_gpio.PUPDR>>24)&3u)==2);
    assert(!tp_f407_status_input_ok());
    fake_gpio.IDR=1u<<12; assert(tp_f407_status_input_ok());
    fake_gpio.IDR=0; assert(!tp_f407_status_input_ok());
    return 0;
}
