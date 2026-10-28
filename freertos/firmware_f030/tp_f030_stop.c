#include "tp_f030_stop.h"
#include "tp_f030_rtc.h"
#include "tp_f030_subordinate.h"
#include "tp_f030_wake_input.h"
#include "stm32f0xx.h"
#include "FreeRTOS.h"
#include "task.h"

#define ALARM_LINE (1u<<17)
#define OTHER_RTC_LINES ((1u<<19)|(1u<<20))
#define RTC_INIT_POLLS 16000000u
#define RTC_ALARM_POLLS 10000u
#define TICK_CYCLES 8000u
static bool ready;
static uint16_t tick_fraction;
static uint16_t us_fraction;

bool tp_f030_stop_init(void) {
    if (ready || SystemCoreClock!=8000000u || !tp_f030_rtc_init(RTC_INIT_POLLS) ||
        (RTC->CR & (RTC_CR_ALRAE|RTC_CR_ALRAIE|RTC_CR_TSIE)) || (RTC->ISR & RTC_ISR_ALRAF) ||
        (EXTI->IMR & OTHER_RTC_LINES) || (EXTI->PR & ALARM_LINE) || NVIC_GetPendingIRQ(RTC_IRQn)) return false;
    EXTI->RTSR |= ALARM_LINE;
    EXTI->FTSR &= ~ALARM_LINE;
    EXTI->IMR |= ALARM_LINE;
    NVIC_SetPriority(RTC_IRQn,2);
    NVIC_EnableIRQ(RTC_IRQn);
    ready=true;
    return true;
}
/* Alarm A is owned by this module. All normal wakeups are acknowledged with
 * interrupts masked; this handler also handles a late alarm interrupt. */
void RTC_IRQHandler(void) {
    if (RTC->ISR & RTC_ISR_ALRAF) {
        if (!tp_f030_rtc_ack_alarm(RTC_ALARM_POLLS)) tp_platform_assert(__FILE__,__LINE__);
    }
}
static bool pending(void) {
    return tp_f030_wake_input_active() ||
        (NVIC->ISPR[0] & NVIC->ISER[0]) ||
        (SCB->ICSR & (SCB_ICSR_PENDSTSET_Msk|SCB_ICSR_PENDSVSET_Msk));
}
static void clear_alarm(void) {
    if (!tp_f030_rtc_ack_alarm(RTC_ALARM_POLLS)) tp_platform_assert(__FILE__,__LINE__);
    NVIC_ClearPendingIRQ(RTC_IRQn);
}
/* Called by the idle task with the scheduler suspended. TIM3 and SysTick are
 * stopped only with interrupts masked. PA8 must be raised before UART traffic. */
void vPortSuppressTicksAndSleep(TickType_t expected) {
    if (!ready || expected<7 || !(TIM3->CR1 & TIM_CR1_CEN)) return;
    if (expected>999) expected=999;
    __disable_irq();
    __DSB(); __ISB();
    if (eTaskConfirmSleepModeStatus()==eAbortSleep || pending() ||
        (RTC->CR & (RTC_CR_ALRAE|RTC_CR_ALRAIE)) || (RTC->ISR & RTC_ISR_ALRAF) ||
        (EXTI->PR & ALARM_LINE)) {
        __enable_irq(); return;
    }
    tp_f030_rtc_stamp armed, start, end;
    /* Reserve four ticks for RTC quantization and setup/restore overhead. */
    if (!tp_f030_rtc_arm((uint32_t)expected-4u,RTC_ALARM_POLLS,&armed)) {
        clear_alarm(); __enable_irq(); return;
    }
    (void)armed;
    if (pending()) { clear_alarm(); __enable_irq(); return; }
    SysTick->CTRL=SysTick_CTRL_CLKSOURCE_Msk|SysTick_CTRL_TICKINT_Msk;
    uint32_t remaining=SysTick->VAL;
    if (!remaining) remaining=TICK_CYCLES;
    if ((SCB->ICSR & SCB_ICSR_PENDSTSET_Msk) || remaining>TICK_CYCLES) {
        SysTick->CTRL=SysTick_CTRL_CLKSOURCE_Msk|SysTick_CTRL_TICKINT_Msk|SysTick_CTRL_ENABLE_Msk;
        clear_alarm(); __enable_irq(); return;
    }
    TIM3->CR1 &= ~TIM_CR1_CEN;
    if (!tp_f030_rtc_read(&start,8)) {
        TIM3->CR1 |= TIM_CR1_CEN;
        SysTick->CTRL=SysTick_CTRL_CLKSOURCE_Msk|SysTick_CTRL_TICKINT_Msk|SysTick_CTRL_ENABLE_Msk;
        clear_alarm(); __enable_irq(); return;
    }
    if (!pending()) {
        PWR->CR &= ~PWR_CR_PDDS;
        PWR->CR |= PWR_CR_LPDS;
        SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
        __DSB(); __WFI(); __ISB();
        SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    }
    /* Account for alarm synchronization while both time bases are stopped. */
    clear_alarm();
    if (!tp_f030_rtc_read(&end,8)) tp_platform_assert(__FILE__,__LINE__);
    uint32_t counts, ticks, next;
    if (!tp_f030_rtc_elapsed(start,end,1023u,&counts) ||
        !tp_f030_rtc_systick(counts,remaining,(uint32_t)expected,&tick_fraction,&ticks,&next))
        tp_platform_assert(__FILE__,__LINE__);
    uint32_t microseconds=counts*976u+(counts*576u+us_fraction)/1024u;
    us_fraction=(uint16_t)((counts*576u+us_fraction)%1024u);
    tp_f030_subordinate_add_sleep_us(microseconds);
    TIM3->CR1 |= TIM_CR1_CEN;
    SysTick->LOAD=next-1u;
    SysTick->VAL=0;
    SysTick->CTRL=SysTick_CTRL_CLKSOURCE_Msk|SysTick_CTRL_TICKINT_Msk|SysTick_CTRL_ENABLE_Msk;
    /* Let the enabled counter load its first, shortened period before
     * restoring the reload value for subsequent ticks. */
    __DSB(); __ISB();
    SysTick->LOAD=TICK_CYCLES-1u;
    vTaskStepTick((TickType_t)ticks);
    __enable_irq();
}
