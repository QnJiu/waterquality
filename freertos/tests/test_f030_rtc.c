#include "tp_f030_rtc.h"
#include "stm32f0xx.h"
#include <assert.h>
RCC_TypeDef rcc; PWR_TypeDef pwr; RTC_TypeDef rtc; EXTI_TypeDef exti;
int main(void) {
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
    rtc.ISR &= ~RTC_ISR_ALRAWF; assert(!tp_f030_rtc_arm(10,2,&stamp));
    assert(rtc.WPR==0xffu && !(rtc.CR & RTC_CR_ALRAE));
    rtc.PRER=0; assert(!tp_f030_rtc_ready() && !tp_f030_rtc_read(&stamp,2));
    return 0;
}
