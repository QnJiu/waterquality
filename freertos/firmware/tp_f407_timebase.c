#include "tp_f407_timebase.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"

tp_result tp_f407_timebase_init(void) {
    static bool initialized;
    if (xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED ||
        SystemCoreClock!=16000000u ||
        (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE1))) return TP_INVALID;
    if (initialized) return TP_OK;
    RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;
    (void)RCC->APB1ENR;
    TIM5->CR1=0; TIM5->PSC=15; TIM5->ARR=0xffffffffu;
    TIM5->EGR=TIM_EGR_UG; TIM5->CNT=0; TIM5->CR1=TIM_CR1_CEN;
    initialized=true;
    return TP_OK;
}
