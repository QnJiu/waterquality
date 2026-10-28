#include <assert.h>
#include "tp_f030_wake_input.h"
#include "stm32f0xx.h"
RCC_TypeDef fake_rcc;
GPIO_TypeDef fake_gpio, fake_gpio_b;
SYSCFG_TypeDef fake_syscfg;
EXTI_TypeDef fake_exti;
static unsigned notifications;
void vTaskNotifyGiveFromISR(TaskHandle_t task,BaseType_t *wake) {
    assert(task && wake); notifications++;
}
void EXTI4_15_IRQHandler(void);
int main(void) {
    tp_f030_subordinate node={0};
    assert(tp_f030_wake_input_active()); /* fail closed until initialized */
    tp_f030_wake_input_start(&node);
    assert(tp_f030_wake_input_active());
    node.task=&node;
    fake_gpio.MODER=0xffffffffu; fake_gpio.PUPDR=0xffffffffu;
    fake_syscfg.EXTICR[2]=0xffffu;
    fake_exti.IMR=1u; fake_exti.RTSR=1u; fake_exti.FTSR=0xffffu;
    fake_gpio.IDR=1u<<8; /* high before interrupt setup must still inhibit sleep */
    tp_f030_wake_input_start(&node);
    assert((fake_gpio.MODER & (3u<<16))==0);
    assert((fake_gpio.PUPDR & (3u<<16))==(2u<<16));
    assert((fake_syscfg.EXTICR[2] & 15u)==0);
    assert((fake_exti.IMR & 0x101u)==0x101u);
    assert((fake_exti.RTSR & 0x101u)==0x101u && !(fake_exti.FTSR & (1u<<8)));
    fake_exti.PR=0; assert(tp_f030_wake_input_active());
    fake_gpio.IDR=0; assert(!tp_f030_wake_input_active());
    fake_exti.PR=1u<<8; assert(tp_f030_wake_input_active());
    EXTI4_15_IRQHandler(); assert(notifications==1 && fake_exti.PR==(1u<<8));
    fake_exti.PR=0; EXTI4_15_IRQHandler(); assert(notifications==1);
    assert(!tp_f030_wake_input_active());
    return 0;
}
