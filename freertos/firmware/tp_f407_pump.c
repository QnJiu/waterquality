#include "tp_f407_pump.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_gpio.h"
#include <string.h>
#define STEP LL_GPIO_PIN_13
#define DIR LL_GPIO_PIN_14
#define EN LL_GPIO_PIN_15
static tp_f407_pump *instance;
static uint32_t irq_lock(void) { uint32_t p=__get_PRIMASK(); __disable_irq(); return p; }
static void irq_unlock(uint32_t p) { __set_PRIMASK(p); }
static void hardware_off(void) {
    LL_GPIO_SetOutputPin(GPIOD,EN);
    TIM4->DIER &= ~TIM_DIER_UIE;
    TIM4->CR1 &= ~TIM_CR1_CEN;
    TIM4->CCER &= ~TIM_CCER_CC2E;
    TIM4->SR=0;
    NVIC_ClearPendingIRQ(TIM4_IRQn);
}
void TIM4_IRQHandler(void) {
    tp_f407_pump *p=instance;
    if (!(TIM4->SR & TIM_SR_UIF)) return;
    TIM4->SR=~TIM_SR_UIF;
    if (!p || p->counter.state!=TP_PULSE_RUNNING) { hardware_off(); return; }
    if (tp_pulse_completed(&p->counter)) hardware_off();
    else TIM4->CR1 |= TIM_CR1_CEN; /* one new cycle, never free-running */
}
tp_result tp_f407_pump_init(tp_f407_pump *p) {
    if (!p || instance || SystemCoreClock!=16000000u ||
        (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1))) return TP_INVALID;
    memset(p,0,sizeof(*p));
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN; RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
    (void)RCC->AHB1ENR; (void)RCC->APB1ENR;
    LL_GPIO_SetOutputPin(GPIOD,EN); LL_GPIO_ResetOutputPin(GPIOD,DIR|STEP);
    LL_GPIO_SetPinOutputType(GPIOD,EN|DIR|STEP,LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinMode(GPIOD,EN,LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinMode(GPIOD,DIR,LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetAFPin_8_15(GPIOD,STEP,LL_GPIO_AF_2);
    RCC->APB1RSTR |= RCC_APB1RSTR_TIM4RST; RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM4RST;
    TIM4->CR1=TIM_CR1_OPM | TIM_CR1_ARPE;
    /* PWM mode 2: low from CNT=0 to CCR2, high until update resets CNT. */
    TIM4->CCMR1=TIM_CCMR1_OC2M | TIM_CCMR1_OC2PE;
    TIM4->PSC=15;
    TIM4->ARR=999; TIM4->CCR2=500; TIM4->EGR=TIM_EGR_UG; TIM4->SR=0;
    LL_GPIO_SetPinMode(GPIOD,STEP,LL_GPIO_MODE_ALTERNATE);
    NVIC_SetPriority(TIM4_IRQn,4u); NVIC_ClearPendingIRQ(TIM4_IRQn);
    instance=p; p->initialized=true; NVIC_EnableIRQ(TIM4_IRQn);
    return TP_OK;
}
tp_result tp_f407_pump_start(tp_f407_pump *p,int32_t steps,uint32_t hz) {
    if (!p || !p->initialized || p!=instance) return TP_INVALID;
    uint32_t primask=irq_lock();
    tp_result r=tp_pulse_begin(&p->counter,steps,hz);
    if (r==TP_OK) {
        hardware_off();
        uint32_t period=1000000u/hz;
        TIM4->ARR=period-1u; TIM4->CCR2=period/2u;
        TIM4->EGR=TIM_EGR_UG; TIM4->SR=0; TIM4->CNT=0;
        if (steps>0) LL_GPIO_SetOutputPin(GPIOD,DIR); else LL_GPIO_ResetOutputPin(GPIOD,DIR);
        TIM4->CCER |= TIM_CCER_CC2E;
        LL_GPIO_ResetOutputPin(GPIOD,EN);
        TIM4->DIER |= TIM_DIER_UIE;
        TIM4->CR1 |= TIM_CR1_CEN;
    }
    irq_unlock(primask); return r;
}
tp_result tp_f407_pump_poll(tp_f407_pump *p,bool *done) {
    if (!p || !p->initialized || !done) return TP_INVALID;
    uint32_t primask=irq_lock(); tp_pulse_state state=p->counter.state; irq_unlock(primask);
    *done=state==TP_PULSE_DONE;
    if (state==TP_PULSE_RUNNING) return TP_BUSY;
    return state==TP_PULSE_DONE ? TP_OK : TP_IO_ERROR;
}
void tp_f407_pump_stop(tp_f407_pump *p) {
    if (!p || !p->initialized || p!=instance) return;
    uint32_t primask=irq_lock(); hardware_off(); tp_pulse_abort(&p->counter); irq_unlock(primask);
}
