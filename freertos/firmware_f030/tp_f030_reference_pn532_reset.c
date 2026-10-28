#include "tp_f030_reference_pn532_reset.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f0xx.h"
#if configTICK_RATE_HZ != 1000
#error "PN532 reset requires a 1 kHz tick"
#endif
_Static_assert(sizeof(TickType_t)>=4,"32-bit ticks required");
#define RESET_PIN (1u<<6)
#define BUS_PINS ((1u<<6)|(1u<<7))
#define RESET_HOLD_MS 20u
#define STARTUP_MS 100u

tp_result tp_f030_reference_pn532_reset_init(tp_f030_pn532_reset *r) {
    if(!r || r->initialized || xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;
    (void)RCC->AHBENR;
    /* Deassert before enabling push-pull output; external pull-up keeps the
     * reader out of reset before GPIO initialization. */
    GPIOA->BSRR=RESET_PIN;
    GPIOA->OTYPER &= ~RESET_PIN;
    GPIOA->PUPDR &= ~(3u<<12);
    GPIOA->OSPEEDR &= ~(3u<<12);
    GPIOA->MODER=(GPIOA->MODER & ~(3u<<12)) | (1u<<12);
    r->phase=0; r->since=0; r->initialized=true;
    return TP_OK;
}
tp_result tp_f030_reference_pn532_reset_step(void *ctx) {
    tp_f030_pn532_reset *r=ctx;
    if(!r || !r->initialized || xTaskGetSchedulerState()!=taskSCHEDULER_RUNNING) return TP_INVALID;
    uint32_t now=(uint32_t)xTaskGetTickCount();
    if(r->phase==0) {
        GPIOA->BSRR=RESET_PIN<<16; r->since=now; r->phase=1; return TP_BUSY;
    }
    if(r->phase==1) {
        if((uint32_t)(now-r->since)<RESET_HOLD_MS) return TP_BUSY;
        GPIOA->BSRR=RESET_PIN; r->since=now; r->phase=2; return TP_BUSY;
    }
    if(r->phase==2) {
        if((uint32_t)(now-r->since)<STARTUP_MS) return TP_BUSY;
        r->phase=0;
        return (GPIOB->IDR & BUS_PINS)==BUS_PINS ? TP_OK : TP_IO_ERROR;
    }
    return TP_INVALID;
}
