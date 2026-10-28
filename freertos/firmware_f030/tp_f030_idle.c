#include "stm32f0xx.h"

/* Shallow sleep keeps SysTick, TIM3 and USART1 running. */
void vApplicationIdleHook(void) {
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    __DSB();
    __WFI();
    __ISB();
}
