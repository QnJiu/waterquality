#include "tp_f030_status_output.h"
#include "stm32f0xx.h"
#include <assert.h>
RCC_TypeDef fake_rcc;
GPIO_TypeDef fake_gpio_b;
static tp_result send(void *ctx,const uint8_t *p,size_t n) { (void)ctx; (void)p; (void)n; return TP_OK; }
static tp_result inputs(void *ctx,tp_sub_inputs *out) { *out=*(tp_sub_inputs *)ctx; return TP_OK; }
int main(void) {
    tp_sub_node node; tp_sub_inputs in={0}; tp_sub_node_io io={&in,send,inputs};
    tp_f030_status_output_fault(); assert(fake_gpio_b.BSRR==0);
    tp_f030_status_output_init();
    assert((fake_rcc.AHBENR & RCC_AHBENR_GPIOBEN) && fake_gpio_b.BSRR==(1u<<16));
    assert((fake_gpio_b.MODER & 3u)==1 && !(fake_gpio_b.OTYPER & 1u));
    tp_f030_status_output_update(NULL); assert(fake_gpio_b.BSRR==(1u<<16));
    assert(tp_sub_node_init(&node,&io,115200,10)==TP_OK);
    tp_f030_status_output_update(&node); assert(fake_gpio_b.BSRR==1);
    in.leak_fault=true; tp_f030_status_output_update(&node); assert(fake_gpio_b.BSRR==(1u<<16));
    in.leak_fault=false; tp_sub_node_count_error(&node);
    tp_f030_status_output_update(&node); assert(fake_gpio_b.BSRR==(1u<<16));
    tp_f030_status_output_fault(); assert(fake_gpio_b.BSRR==(1u<<16));
    return 0;
}
