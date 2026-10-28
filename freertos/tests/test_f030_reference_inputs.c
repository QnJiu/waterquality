#include <assert.h>
#include "tp_f030_reference_inputs.h"
#include "tp_f030_board_inputs.h"
#include "stm32f0xx.h"
RCC_TypeDef fake_rcc;
GPIO_TypeDef fake_gpio, fake_gpio_b;
SYSCFG_TypeDef fake_syscfg;
EXTI_TypeDef fake_exti;
static unsigned notifications, droplets;
void vTaskNotifyGiveFromISR(TaskHandle_t task,BaseType_t *wake) {
    assert(task && wake); notifications++;
}
void tp_f030_subordinate_droplet_from_isr(tp_f030_subordinate *s,BaseType_t *wake) {
    assert(s && s->task && wake); droplets++;
}
void EXTI0_1_IRQHandler(void);
void EXTI2_3_IRQHandler(void);
int main(void) {
    tp_sub_inputs inputs; tp_f030_subordinate subordinate={0};
    assert(tp_f030_board_inputs(NULL,&inputs)==TP_INVALID);
    subordinate.task=&subordinate;
    tp_f030_reference_inputs_start(&subordinate);
    assert((fake_exti.IMR & 15u)==15u && (fake_exti.RTSR & 15u)==11u && (fake_exti.FTSR & 15u)==7u);
    assert((fake_syscfg.EXTICR[0] & 0xffffu)==SYSCFG_EXTICR1_EXTI2_PB);
    assert((fake_gpio.PUPDR & 0xcfu)==0x45u && (fake_gpio_b.PUPDR & (3u<<4))==(1u<<4));
    fake_gpio.IDR=0; assert(tp_f030_board_inputs(NULL,&inputs)==TP_OK);
    assert(!inputs.level_fault && !inputs.reagent_fault && !inputs.device_fault && !inputs.leak_fault);
    fake_gpio.IDR=3; assert(tp_f030_board_inputs(NULL,&inputs)==TP_OK);
    assert(inputs.level_fault && inputs.reagent_fault && !inputs.device_fault);
    fake_exti.PR=1; EXTI0_1_IRQHandler(); assert(notifications==1 && fake_exti.PR==1);
    fake_exti.PR=0; EXTI0_1_IRQHandler(); assert(notifications==1);
    fake_exti.PR=4; EXTI2_3_IRQHandler(); assert(droplets==1 && fake_exti.PR==4);
    fake_exti.PR=0; EXTI2_3_IRQHandler(); assert(droplets==1);
    fake_exti.PR=8; EXTI2_3_IRQHandler(); assert(notifications==2 && fake_exti.PR==8);
    fake_gpio.IDR=0; assert(tp_f030_board_inputs(NULL,&inputs)==TP_OK && inputs.leak_fault);
    return 0;
}
