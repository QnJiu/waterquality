#include "tp_f030_reference_inputs.h"
#include "tp_f030_board_inputs.h"
#include "stm32f0xx.h"

#define LEVEL (1u<<0)
#define REAGENT (1u<<1)
#define DROPLET (1u<<2)
#define LEAK (1u<<3)
#define INPUT_LINES (LEVEL | REAGENT | DROPLET | LEAK)
static tp_f030_subordinate *owner;
static volatile bool leak_latched;

void tp_f030_reference_inputs_start(tp_f030_subordinate *s) {
    if(!s || !s->task || owner) return;
    RCC->AHBENR|=RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;
    RCC->APB2ENR|=RCC_APB2ENR_SYSCFGEN;
    (void)RCC->AHBENR; (void)RCC->APB2ENR;
    EXTI->IMR &= ~INPUT_LINES;
    GPIOA->MODER &= ~((3u<<0)|(3u<<2)|(3u<<6));
    GPIOA->PUPDR=(GPIOA->PUPDR & ~((3u<<0)|(3u<<2)|(3u<<6))) | (1u<<0)|(1u<<2)|(1u<<6);
    GPIOB->MODER &= ~(3u<<4);
    GPIOB->PUPDR=(GPIOB->PUPDR & ~(3u<<4)) | (1u<<4);
    SYSCFG->EXTICR[0]=(SYSCFG->EXTICR[0] & ~0xffffu) | SYSCFG_EXTICR1_EXTI2_PB;
    EXTI->RTSR=(EXTI->RTSR & ~INPUT_LINES) | LEVEL | REAGENT | LEAK;
    EXTI->FTSR=(EXTI->FTSR & ~INPUT_LINES) | LEVEL | REAGENT | DROPLET;
    EXTI->PR=INPUT_LINES;
    leak_latched=(GPIOA->IDR & LEAK)!=0;
    owner=s;
    NVIC_SetPriority(EXTI0_1_IRQn,2); NVIC_ClearPendingIRQ(EXTI0_1_IRQn);
    NVIC_SetPriority(EXTI2_3_IRQn,2); NVIC_ClearPendingIRQ(EXTI2_3_IRQn);
    EXTI->IMR |= INPUT_LINES;
    NVIC_EnableIRQ(EXTI0_1_IRQn); NVIC_EnableIRQ(EXTI2_3_IRQn);
}
tp_result tp_f030_reference_inputs_acknowledge_leak(void) {
    if(!owner || xTaskGetSchedulerState()!=taskSCHEDULER_RUNNING) return TP_INVALID;
    taskENTER_CRITICAL();
    if((GPIOA->IDR & LEAK) || (EXTI->PR & LEAK)) {
        taskEXIT_CRITICAL(); return TP_BUSY;
    }
    leak_latched=false;
    taskEXIT_CRITICAL();
    return TP_OK;
}
tp_result tp_f030_board_inputs(void *ctx,tp_sub_inputs *out) {
    (void)ctx;
    if(!owner || !out) return TP_INVALID;
    uint32_t pins=GPIOA->IDR;
    *out=(tp_sub_inputs){ .reagent_fault=(pins & REAGENT)!=0,
                          .level_fault=(pins & LEVEL)!=0, .device_fault=false,
                          .leak_fault=leak_latched || (pins & LEAK)!=0 };
    return TP_OK;
}
void EXTI0_1_IRQHandler(void) {
    uint32_t pending=EXTI->PR & (LEVEL | REAGENT);
    if(pending) {
        EXTI->PR=pending;
        if(owner) { BaseType_t wake=pdFALSE; vTaskNotifyGiveFromISR(owner->task,&wake); portYIELD_FROM_ISR(wake); }
    }
}
void EXTI2_3_IRQHandler(void) {
    uint32_t pending=EXTI->PR & (DROPLET | LEAK);
    if (!pending) return;
    EXTI->PR=pending;
    if(pending & LEAK) leak_latched=true;
    if(owner) {
        BaseType_t wake=pdFALSE;
        if(pending & LEAK) vTaskNotifyGiveFromISR(owner->task,&wake);
        if(pending & DROPLET) tp_f030_subordinate_droplet_from_isr(owner,&wake);
        portYIELD_FROM_ISR(wake);
    }
}