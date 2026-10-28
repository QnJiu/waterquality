#include "tp_f030_status_output.h"
#include "stm32f0xx.h"
#define PIN (1u<<0)
static bool initialized;

void tp_f030_status_output_init(void) {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    (void)RCC->AHBENR;
    GPIOB->BSRR=PIN<<16; /* fault until the first successful snapshot */
    GPIOB->OTYPER &= ~PIN;
    GPIOB->PUPDR &= ~3u;
    GPIOB->OSPEEDR &= ~3u;
    GPIOB->MODER=(GPIOB->MODER & ~3u) | 1u;
    initialized=true;
}
void tp_f030_status_output_fault(void) {
    if(initialized) GPIOB->BSRR=PIN<<16;
}
void tp_f030_status_output_update(tp_sub_node *node) {
    if(!initialized) return;
    tp_sub_status status;
    GPIOB->BSRR=(tp_sub_node_snapshot(node,&status)==TP_OK && !status.faults) ? PIN : PIN<<16;
}
