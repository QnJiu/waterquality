#include <assert.h>
#include "tp_f030_reference_inputs.h"
#include "tp_f030_board_inputs.h"
#include "stm32f0xx.h"
RCC_TypeDef fake_rcc;
GPIO_TypeDef fake_gpio, fake_gpio_b;
SYSCFG_TypeDef fake_syscfg;
EXTI_TypeDef fake_exti;
static unsigned notifications, droplets;
static BaseType_t scheduler=taskSCHEDULER_RUNNING;
BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
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
    assert(tp_f030_reference_inputs_acknowledge_leak()==TP_INVALID);
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
    scheduler=taskSCHEDULER_NOT_STARTED;
    assert(tp_f030_reference_inputs_acknowledge_leak()==TP_INVALID);
    scheduler=taskSCHEDULER_RUNNING;
    fake_gpio.IDR=8; fake_exti.PR=0;
    assert(tp_f030_reference_inputs_acknowledge_leak()==TP_BUSY);
    fake_gpio.IDR=0; fake_exti.PR=8;
    assert(tp_f030_reference_inputs_acknowledge_leak()==TP_BUSY);
    assert(tp_f030_board_inputs(NULL,&inputs)==TP_OK && inputs.leak_fault);
    fake_exti.PR=0;
    assert(tp_f030_reference_inputs_acknowledge_leak()==TP_OK);
    assert(tp_f030_board_inputs(NULL,&inputs)==TP_OK && !inputs.leak_fault);
    fake_exti.PR=8; EXTI2_3_IRQHandler(); fake_exti.PR=0;
    assert(tp_f030_board_inputs(NULL,&inputs)==TP_OK && inputs.leak_fault);
    /* A held button at startup must never acknowledge the latched fault. */
    fake_gpio.IDR=0; tp_f030_reference_inputs_button_tick(0);
    tp_f030_reference_inputs_button_tick(5000);
    assert(tp_f030_board_inputs(NULL,&inputs)==TP_OK && inputs.leak_fault);
    /* A stable release arms it, then a two-second hold acknowledges once. */
    fake_gpio.IDR=16; tp_f030_reference_inputs_button_tick(5100);
    tp_f030_reference_inputs_button_tick(5200);
    fake_gpio.IDR=0; tp_f030_reference_inputs_button_tick(5300);
    tp_f030_reference_inputs_button_tick(7299);
    assert(tp_f030_board_inputs(NULL,&inputs)==TP_OK && inputs.leak_fault);
    tp_f030_reference_inputs_button_tick(7300);
    assert(tp_f030_board_inputs(NULL,&inputs)==TP_OK && !inputs.leak_fault);
    /* A new leak while held is not automatically cleared. */
    fake_exti.PR=8; EXTI2_3_IRQHandler(); fake_exti.PR=0;
    tp_f030_reference_inputs_button_tick(10000);
    assert(tp_f030_board_inputs(NULL,&inputs)==TP_OK && inputs.leak_fault);
    /* A press while wet fails; drying without another press cannot clear it. */
    fake_gpio.IDR=16; tp_f030_reference_inputs_button_tick(10100);
    tp_f030_reference_inputs_button_tick(10200);
    fake_gpio.IDR=8; tp_f030_reference_inputs_button_tick(10300);
    tp_f030_reference_inputs_button_tick(12300);
    fake_gpio.IDR=0; tp_f030_reference_inputs_button_tick(15000);
    assert(tp_f030_board_inputs(NULL,&inputs)==TP_OK && inputs.leak_fault);
    /* Tick wrap is safe for both release and press timers. */
    fake_gpio.IDR=16; tp_f030_reference_inputs_button_tick(UINT32_MAX-50u);
    tp_f030_reference_inputs_button_tick(49);
    fake_gpio.IDR=0; tp_f030_reference_inputs_button_tick(50);
    tp_f030_reference_inputs_button_tick(2050);
    assert(tp_f030_board_inputs(NULL,&inputs)==TP_OK && !inputs.leak_fault);
    return 0;
}
