#include "tp_f407_valves.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_gpio.h"
#define VALVE_B 0x03f8u
#define VALVE_D 0x00fcu
#define VALVE_E 0x003fu
static void configure_pins(GPIO_TypeDef *gpio,uint32_t mask) {
    for (uint32_t pin=1;pin<=0x8000u;pin<<=1) if (mask&pin) {
        LL_GPIO_SetPinOutputType(gpio,pin,LL_GPIO_OUTPUT_PUSHPULL);
        LL_GPIO_SetPinPull(gpio,pin,LL_GPIO_PULL_NO);
        LL_GPIO_SetPinMode(gpio,pin,LL_GPIO_MODE_OUTPUT);
    }
}
static void all_off(void) {
    GPIOB->BSRR=VALVE_B; GPIOD->BSRR=VALVE_D; GPIOE->BSRR=VALVE_E;
}
tp_result tp_f407_valves_init(tp_f407_valves *v,const tp_valve_policy *p) {
    if (!v || tp_valve_policy_validate(p)!=TP_OK) return TP_INVALID;
    v->initialized=false; v->policy=*p;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN;
    (void)RCC->AHB1ENR;
    all_off(); /* preload inactive levels before switching pins to outputs */
    configure_pins(GPIOB,VALVE_B); configure_pins(GPIOD,VALVE_D); configure_pins(GPIOE,VALVE_E);
    v->initialized=true; return TP_OK;
}
tp_result tp_f407_valves_set(tp_f407_valves *v,uint32_t mask) {
    if (!v || !v->initialized) return TP_INVALID;
    if (!tp_valve_policy_allows(&v->policy,mask)) { all_off(); return TP_INVALID; }
    uint16_t b,d,e;
    if (tp_valve_reference_pins(mask,&b,&d,&e)!=TP_OK) { all_off(); return TP_INVALID; }
    /* Break before make: never open a new path before old paths are off. */
    all_off();
    GPIOB->BSRR=(uint32_t)b<<16; GPIOD->BSRR=(uint32_t)d<<16; GPIOE->BSRR=(uint32_t)e<<16;
    return TP_OK;
}
void tp_f407_valves_safe(tp_f407_valves *v) { if (v && v->initialized) all_off(); }
