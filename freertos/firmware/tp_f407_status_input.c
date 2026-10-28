#include "tp_f407_status_input.h"
#include "stm32f4xx.h"
#define PIN (1u<<12)
void tp_f407_status_input_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;
    GPIOC->PUPDR=(GPIOC->PUPDR & ~(3u<<24)) | (2u<<24);
    GPIOC->MODER &= ~(3u<<24);
}
bool tp_f407_status_input_ok(void) { return (GPIOC->IDR & PIN)!=0; }
