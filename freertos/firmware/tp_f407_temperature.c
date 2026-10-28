#include "tp_f407_temperature.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_gpio.h"
#if configTICK_RATE_HZ != 1000
#error "temperature sampling requires a 1 kHz RTOS tick"
#endif
_Static_assert(sizeof(TickType_t)>=4,"32-bit ticks required");
static tp_f407_temperature *instance;
static void adc_startup(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->AHB1ENR; (void)RCC->APB2ENR;
    LL_GPIO_SetPinPull(GPIOA,LL_GPIO_PIN_0,LL_GPIO_PULL_NO);
    LL_GPIO_SetPinMode(GPIOA,LL_GPIO_PIN_0,LL_GPIO_MODE_ANALOG);
    RCC->APB2RSTR |= RCC_APB2RSTR_ADCRST; RCC->APB2RSTR &= ~RCC_APB2RSTR_ADCRST;
    ADC->CCR &= ~ADC_CCR_ADCPRE; /* APB2 / 2 = 8 MHz */
    ADC1->CR1=0; ADC1->CR2=ADC_CR2_ADON;
    ADC1->SMPR2=ADC_SMPR2_SMP0; /* 480 cycles */
    ADC1->SQR1=0; ADC1->SQR3=0;
}
static void temperature_task(void *arg) {
    tp_f407_temperature *s=arg;
    adc_startup();
    vTaskDelay(pdMS_TO_TICKS(1)); /* ADC startup */
    for (;;) {
        tp_temperature_sample sample={0};
        ADC1->SR=0; ADC1->CR2 |= ADC_CR2_SWSTART;
        uint32_t started=(uint32_t)xTaskGetTickCount();
        while (!(ADC1->SR & ADC_SR_EOC) && (uint32_t)(xTaskGetTickCount()-started)<10u)
            vTaskDelay(pdMS_TO_TICKS(1));
        if ((ADC1->SR & (ADC_SR_EOC | ADC_SR_OVR))==ADC_SR_EOC) {
            uint16_t raw=(uint16_t)ADC1->DR;
            sample.valid=tp_ntc_10k_3950_celsius(raw,&sample.celsius)==TP_OK;
        } else {
            /* Stop a stuck conversion before the next attempt. */
            ADC1->CR2=0; ADC1->SR=0; ADC1->CR2=ADC_CR2_ADON;
        }
        sample.at_ms=(uint32_t)xTaskGetTickCount();
        (void)xQueueOverwrite(s->samples,&sample);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
tp_result tp_f407_temperature_start(tp_f407_temperature *s,uint32_t max_age,
                                    uint16_t stack,UBaseType_t priority) {
    if (!s || instance || max_age<50u || max_age>0x7fffffffu || !stack || priority>=configMAX_PRIORITIES ||
        SystemCoreClock!=16000000u || (RCC->CFGR & (RCC_CFGR_SWS | RCC_CFGR_HPRE | RCC_CFGR_PPRE2)) ||
        xTaskGetSchedulerState()!=taskSCHEDULER_NOT_STARTED) return TP_INVALID;
    s->samples=xQueueCreate(1,sizeof(tp_temperature_sample));
    if (!s->samples) return TP_IO_ERROR;
    s->max_age_ms=max_age; s->task=NULL; instance=s;
    if (xTaskCreate(temperature_task,"tp-temp",stack,s,priority,&s->task)!=pdPASS) {
        instance=NULL; vQueueDelete(s->samples); s->samples=NULL; return TP_IO_ERROR;
    }
    return TP_OK;
}
tp_result tp_f407_temperature_read(tp_f407_temperature *s,float *temperature) {
    tp_temperature_sample sample;
    if (!s || s!=instance || !temperature || !s->samples) return TP_INVALID;
    if (xQueuePeek(s->samples,&sample,0)!=pdPASS || !sample.valid ||
        (uint32_t)((uint32_t)xTaskGetTickCount()-sample.at_ms)>s->max_age_ms) return TP_IO_ERROR;
    *temperature=sample.celsius; return TP_OK;
}
