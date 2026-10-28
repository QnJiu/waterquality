#include "tp_f030_wake_input.h"
#include "stm32f0xx.h"

#define WAKE_LINE (1u << 8)
static tp_f030_subordinate *owner;

void tp_f030_wake_input_start(tp_f030_subordinate *s) {
    if(!s || !s->task || owner) return;
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->AHBENR; (void)RCC->APB2ENR;
    EXTI->IMR &= ~WAKE_LINE;
    GPIOA->MODER &= ~(3u << 16);
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(3u << 16)) | (2u << 16);
    SYSCFG->EXTICR[2] &= ~15u; /* PA8 */
    EXTI->RTSR |= WAKE_LINE;
    EXTI->FTSR &= ~WAKE_LINE;
    EXTI->PR = WAKE_LINE;
    owner=s;
    NVIC_SetPriority(EXTI4_15_IRQn,2);
    NVIC_ClearPendingIRQ(EXTI4_15_IRQn);
    EXTI->IMR |= WAKE_LINE;
    NVIC_EnableIRQ(EXTI4_15_IRQn);
}
bool tp_f030_wake_input_active(void) {
    return !owner || ((GPIOA->IDR | EXTI->PR) & WAKE_LINE)!=0;
}
void EXTI4_15_IRQHandler(void) {
    if(EXTI->PR & WAKE_LINE) {
        EXTI->PR=WAKE_LINE;
        if(owner) {
            BaseType_t wake=pdFALSE;
            vTaskNotifyGiveFromISR(owner->task,&wake);
            portYIELD_FROM_ISR(wake);
        }
    }
}
