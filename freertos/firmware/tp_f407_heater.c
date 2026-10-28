#include "tp_f407_heater.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_gpio.h"
#define HEAT_PIN LL_GPIO_PIN_15
#define PERIOD_TICKS 1000000u
static tp_f407_heater *instance;
tp_result tp_f407_heater_init(tp_f407_heater *h) {
    if (!h || instance || SystemCoreClock!=16000000u ||
        (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1))) return TP_INVALID;
    h->initialized=false; h->active=false;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->AHB1ENR; (void)RCC->APB1ENR;
    LL_GPIO_SetOutputPin(GPIOA,HEAT_PIN);
    LL_GPIO_SetPinOutputType(GPIOA,HEAT_PIN,LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinMode(GPIOA,HEAT_PIN,LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetAFPin_8_15(GPIOA,HEAT_PIN,LL_GPIO_AF_1);
    RCC->APB1RSTR |= RCC_APB1RSTR_TIM2RST; RCC->APB1RSTR &= ~RCC_APB1RSTR_TIM2RST;
    TIM2->PSC=15; TIM2->ARR=PERIOD_TICKS-1u; TIM2->CCR1=0;
    TIM2->CCMR1=TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1; /* PWM1, no preload */
    TIM2->CCER=TIM_CCER_CC1P | TIM_CCER_CC1E; /* inverted, active low */
    TIM2->EGR=TIM_EGR_UG; TIM2->SR=0;
    h->initialized=true; instance=h; return TP_OK;
}
void tp_f407_heater_off(tp_f407_heater *h) {
    if (!h || !h->initialized || h!=instance) return;
    LL_GPIO_SetOutputPin(GPIOA,HEAT_PIN);
    LL_GPIO_SetPinMode(GPIOA,HEAT_PIN,LL_GPIO_MODE_OUTPUT);
    TIM2->CR1 &= ~TIM_CR1_CEN; TIM2->CCR1=0; h->active=false;
}
tp_result tp_f407_heater_set(tp_f407_heater *h,float fraction) {
    if (!h || !h->initialized || h!=instance) return TP_INVALID;
    uint32_t ticks;
    if (tp_heater_duty_ticks(fraction,PERIOD_TICKS,&ticks)!=TP_OK) {
        tp_f407_heater_off(h); return TP_INVALID;
    }
    if (!ticks) { tp_f407_heater_off(h); return TP_OK; }
    if (!h->active) {
        TIM2->CCR1=0; TIM2->CNT=0; TIM2->CR1 |= TIM_CR1_CEN;
        LL_GPIO_SetPinMode(GPIOA,HEAT_PIN,LL_GPIO_MODE_ALTERNATE);
        h->active=true;
    }
    TIM2->CCR1=ticks;
    return TP_OK;
}
