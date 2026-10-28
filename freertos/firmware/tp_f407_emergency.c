#include "tp_f407_emergency.h"
#include "stm32f4xx.h"

/* Low-active valves, heater, and pump enable. Preload inactive output levels
 * before changing alternate-function pins to GPIO. No RTOS or driver state is
 * touched: running drivers cannot resume after this function. It may also be
 * called at reset, before any driver is initialized. */
static void output_mode(GPIO_TypeDef *port, unsigned pin) {
    port->OTYPER &= ~(1u << pin);
    port->MODER = (port->MODER & ~(3u << (2u * pin))) | (1u << (2u * pin));
}
void tp_f407_emergency_off(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN |
                     RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN;
    (void)RCC->AHB1ENR;
    GPIOA->BSRR = 1u << 15; /* heater off */
    GPIOB->BSRR = 0x03f8u;
    GPIOD->BSRR = 0x80fcu; /* pump disabled; valves off */
    GPIOE->BSRR = 0x003fu;
    output_mode(GPIOA, 15);
    for (unsigned pin = 3; pin <= 9; ++pin) output_mode(GPIOB, pin);
    for (unsigned pin = 2; pin <= 7; ++pin) output_mode(GPIOD, pin);
    output_mode(GPIOD, 15);
    for (unsigned pin = 0; pin <= 5; ++pin) output_mode(GPIOE, pin);
    /* Stop the pulse output as well if its timer clock is enabled. */
    if (RCC->APB1ENR & RCC_APB1ENR_TIM4EN) {
        TIM4->DIER = 0;
        TIM4->CR1 &= ~TIM_CR1_CEN;
        TIM4->CCER &= ~TIM_CCER_CC2E;
    }
    if (RCC->APB1ENR & RCC_APB1ENR_TIM2EN) TIM2->CR1 &= ~TIM_CR1_CEN;
    __DSB();
}
