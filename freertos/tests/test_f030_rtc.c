#include "tp_f030_rtc.h"
#include "stm32f0xx.h"
#include <assert.h>
RCC_TypeDef rcc; PWR_TypeDef pwr; RTC_TypeDef rtc; EXTI_TypeDef exti;
static int simulate_clock;
void rtc_test_clear_alarm(void) { rtc.ISR &= ~RTC_ISR_ALRAF; }
void rtc_test_clear_exti(void) { exti.PR &= ~EXTI_PR_PR17; }
void rtc_test_wait_hook(void) {
    if (!simulate_clock) return;
    if (rcc.BDCR & RCC_BDCR_LSEON) rcc.BDCR |= RCC_BDCR_LSERDY;
    if (rtc.ISR & RTC_ISR_INIT) rtc.ISR |= RTC_ISR_INITF;
    else if (rtc.DR) rtc.ISR |= RTC_ISR_INITS;
}
int main(void) {
    assert(!tp_f030_rtc_init(0));
    assert(!tp_f030_rtc_init(2)); /* LSE did not start */
    assert(!(rcc.BDCR & RCC_BDCR_RTCEN));
    rcc.BDCR=2u<<8; assert(!tp_f030_rtc_init(2)); /* other clock */
    rcc.BDCR=0; simulate_clock=1;
    assert(tp_f030_rtc_init(8));
    assert(rtc.PRER==((31u<<16)|1023u) && rtc.TR==0 && rtc.DR==0x12101u);
    assert((rtc.CR & RTC_CR_BYPSHAD) && rtc.WPR==0xffu);
    rtc.DR=0x31201u; assert(tp_f030_rtc_init(8) && rtc.DR==0x31201u);
    rtc.CR |= RTC_CR_FMT; assert(!tp_f030_rtc_init(8) && rtc.DR==0x31201u);
    rcc=(RCC_TypeDef){0}; pwr=(PWR_TypeDef){0}; rtc=(RTC_TypeDef){0}; exti=(EXTI_TypeDef){0};
    simulate_clock=0; rcc.BDCR=RCC_BDCR_LSERDY;
    assert(!tp_f030_rtc_init(2)); /* INITF never arrived */
    assert(!(rtc.ISR & RTC_ISR_INIT) && rtc.WPR==0xffu);
    rcc=(RCC_TypeDef){0}; pwr=(PWR_TypeDef){0}; rtc=(RTC_TypeDef){0}; exti=(EXTI_TypeDef){0};
    tp_f030_rtc_stamp stamp={99,999};
    rcc.BDCR=RCC_BDCR_RTCEN|RCC_BDCR_RTCSEL_LSE|RCC_BDCR_LSERDY;
    pwr.CR=PWR_CR_DBP; rtc.CR=RTC_CR_BYPSHAD;
    rtc.ISR=RTC_ISR_INITS|RTC_ISR_ALRAWF; rtc.PRER=(31u<<16)|1023u;
    rtc.TR=0x59; rtc.SSR=3; exti.IMR=exti.RTSR=EXTI_PR_PR17;
    assert(tp_f030_rtc_ready());
    assert(tp_f030_rtc_read(&stamp,2) && stamp.second==59 && stamp.subsecond==3);
    rtc.TR=0x6a; assert(!tp_f030_rtc_read(&stamp,2)); rtc.TR=0x59;
    assert(!tp_f030_rtc_arm(1,100,&stamp));
    assert(!(rtc.CR & RTC_CR_ALRAE));
    assert(tp_f030_rtc_arm(10,100,&stamp));
    assert(rtc.ALRMASSR==((15u<<24)|1017u));
    assert(rtc.ALRMAR==0x80808080u && (rtc.CR & RTC_CR_ALRAE));
    assert(!tp_f030_rtc_arm(10,100,&stamp));
    tp_f030_rtc_disarm(); assert(!(rtc.CR & (RTC_CR_ALRAE|RTC_CR_ALRAIE)));
    exti.PR=EXTI_PR_PR17; assert(!tp_f030_rtc_arm(10,100,&stamp));
    assert(exti.PR==EXTI_PR_PR17); exti.PR=0;
    rtc.ISR|=RTC_ISR_ALRAF; assert(!tp_f030_rtc_arm(10,100,&stamp));
    assert(rtc.ISR & RTC_ISR_ALRAF); rtc.ISR &= ~RTC_ISR_ALRAF;
    rtc.ISR &= ~RTC_ISR_ALRAWF;
    rtc.ISR |= RTC_ISR_ALRAF; exti.PR=EXTI_PR_PR17|(1u<<3);
    assert(!tp_f030_rtc_ack_alarm(2));
    assert((rtc.ISR & RTC_ISR_ALRAF) && (exti.PR & EXTI_PR_PR17));
    rtc.ISR |= RTC_ISR_ALRAWF | (1u<<13); rtc.CR |= RTC_CR_ALRAE|RTC_CR_ALRAIE;
    assert(tp_f030_rtc_ack_alarm(2));
    assert(!(rtc.CR & (RTC_CR_ALRAE|RTC_CR_ALRAIE)) && !(rtc.ISR & RTC_ISR_ALRAF));
    assert((rtc.ISR & (1u<<13)) && exti.PR==(1u<<3));
    rtc.ISR &= ~RTC_ISR_ALRAWF; assert(!tp_f030_rtc_arm(10,2,&stamp));
    assert(rtc.WPR==0xffu && !(rtc.CR & RTC_CR_ALRAE));
    rtc.PRER=0; assert(!tp_f030_rtc_ready() && !tp_f030_rtc_read(&stamp,2));
    return 0;
}
