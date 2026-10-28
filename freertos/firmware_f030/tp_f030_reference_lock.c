#include "tp_f030_reference_lock.h"
#include "stm32f0xx.h"
#define LOCK_PIN (1u<<5)
static bool initialized;

void tp_f030_reference_lock_emergency_off(void) {
    if(initialized) GPIOA->BSRR=LOCK_PIN<<16;
}
static tp_result set_unlocked(void *ctx,bool unlocked) {
    (void)ctx;
    if(!initialized) return TP_INVALID;
    GPIOA->BSRR=unlocked ? LOCK_PIN : LOCK_PIN<<16;
    return TP_OK;
}
tp_result tp_f030_reference_lock_init(void) {
    if(initialized) return TP_INVALID;
    RCC->AHBENR|=RCC_AHBENR_GPIOAEN;
    (void)RCC->AHBENR;
    /* Set the output latch before switching from reset input to output. */
    GPIOA->BSRR=LOCK_PIN<<16;
    GPIOA->OTYPER &= ~LOCK_PIN;
    GPIOA->PUPDR &= ~(3u<<10);
    GPIOA->OSPEEDR &= ~(3u<<10);
    GPIOA->MODER=(GPIOA->MODER & ~(3u<<10)) | (1u<<10);
    initialized=true;
    return TP_OK;
}
tp_door_io tp_f030_reference_lock_io(void) {
    tp_door_io io={NULL,set_unlocked}; return io;
}
