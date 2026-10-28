#include "tp_f030_reference_pn532_reset.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f0xx.h"
#include <assert.h>
RCC_TypeDef fake_rcc; GPIO_TypeDef fake_gpio, fake_gpio_b;
static TickType_t tick;
static BaseType_t scheduler=taskSCHEDULER_NOT_STARTED;
BaseType_t xTaskGetSchedulerState(void) { return scheduler; }
TickType_t xTaskGetTickCount(void) { return tick; }
int main(void) {
    tp_f030_pn532_reset r={0};
    assert(tp_f030_reference_pn532_reset_step(&r)==TP_INVALID);
    assert(tp_f030_reference_pn532_reset_init(&r)==TP_OK);
    assert(fake_gpio.BSRR==(1u<<6) && ((fake_gpio.MODER>>12)&3u)==1);
    assert(tp_f030_reference_pn532_reset_init(&r)==TP_INVALID);
    assert(tp_f030_reference_pn532_reset_step(&r)==TP_INVALID);
    scheduler=taskSCHEDULER_RUNNING; tick=0xfffffff8u;
    assert(tp_f030_reference_pn532_reset_step(&r)==TP_BUSY && fake_gpio.BSRR==(1u<<22));
    tick=11; assert(tp_f030_reference_pn532_reset_step(&r)==TP_BUSY && fake_gpio.BSRR==(1u<<22));
    tick=12; assert(tp_f030_reference_pn532_reset_step(&r)==TP_BUSY && fake_gpio.BSRR==(1u<<6));
    tick=111; assert(tp_f030_reference_pn532_reset_step(&r)==TP_BUSY);
    tick=112; fake_gpio_b.IDR=(1u<<6)|(1u<<7);
    assert(tp_f030_reference_pn532_reset_step(&r)==TP_OK);
    assert(tp_f030_reference_pn532_reset_step(&r)==TP_BUSY);
    tick+=20; assert(tp_f030_reference_pn532_reset_step(&r)==TP_BUSY);
    fake_gpio_b.IDR=(1u<<6); tick+=100;
    assert(tp_f030_reference_pn532_reset_step(&r)==TP_IO_ERROR);
    assert(tp_f030_reference_pn532_reset_step(&r)==TP_BUSY);
    return 0;
}
