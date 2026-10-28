#include "tp_f030_rtc.h"
#include "stm32f0xx.h"
#include <stddef.h>

#define ALARM_LINE EXTI_PR_PR17
#define ALL_FIELDS (RTC_ALRMAR_MSK1|RTC_ALRMAR_MSK2|RTC_ALRMAR_MSK3|RTC_ALRMAR_MSK4)
#define PRESCALERS ((31u<<16)|1023u)
static void unlock(void) { RTC->WPR=0xcau; RTC->WPR=0x53u; }
static void lock(void) { RTC->WPR=0xffu; }
bool tp_f030_rtc_ready(void) {
    return (RCC->BDCR & (RCC_BDCR_RTCEN|RCC_BDCR_RTCSEL|RCC_BDCR_LSERDY))==
        (RCC_BDCR_RTCEN|RCC_BDCR_RTCSEL_LSE|RCC_BDCR_LSERDY) &&
        (PWR->CR & PWR_CR_DBP) && (RTC->ISR & RTC_ISR_INITS) &&
        (RTC->CR & RTC_CR_BYPSHAD) && (RTC->PRER & (RTC_PRER_PREDIV_A|RTC_PRER_PREDIV_S))==PRESCALERS;
}
bool tp_f030_rtc_read(tp_f030_rtc_stamp *stamp,uint32_t attempts) {
    if (!stamp || !attempts || !tp_f030_rtc_ready()) return false;
    while (attempts--) {
        uint32_t s1=RTC->SSR, t1=RTC->TR, s2=RTC->SSR, t2=RTC->TR;
        if (s1!=s2 || (t1 & 0x7fu)!=(t2 & 0x7fu)) continue;
        uint32_t unit=t2 & 0xfu, tens=(t2>>4)&7u;
        if (s2>1023u || unit>9u || tens>5u) return false;
        *stamp=(tp_f030_rtc_stamp){(uint8_t)(tens*10u+unit),(uint16_t)s2};
        return true;
    }
    return false;
}
void tp_f030_rtc_disarm(void) {
    unlock(); RTC->CR &= ~(RTC_CR_ALRAE|RTC_CR_ALRAIE); lock();
}
bool tp_f030_rtc_arm(uint32_t budget_ms,uint32_t poll_limit,tp_f030_rtc_stamp *started) {
    if (!started || !poll_limit || !tp_f030_rtc_ready() ||
        !(EXTI->IMR & ALARM_LINE) || !(EXTI->RTSR & ALARM_LINE) ||
        (EXTI->PR & ALARM_LINE) || (RTC->ISR & RTC_ISR_ALRAF) ||
        (RTC->CR & RTC_CR_ALRAE)) return false;
    unlock();
    while (!(RTC->ISR & RTC_ISR_ALRAWF)) {
        if (!--poll_limit) { lock(); return false; }
    }
    tp_f030_rtc_stamp before, after;
    uint16_t target;
    if (!tp_f030_rtc_read(&before,8) ||
        !tp_f030_rtc_alarm_target(before.subsecond,budget_ms,&target)) { lock(); return false; }
    RTC->ALRMAR=ALL_FIELDS;
    RTC->ALRMASSR=(15u<<RTC_ALRMASSR_MASKSS_Pos)|target;
    RTC->CR |= RTC_CR_ALRAIE|RTC_CR_ALRAE;
    lock();
    /* Keep at least one complete subsecond period for alarm-enable clock
     * synchronization; a target at the very next edge may already be missed. */
    uint32_t elapsed, distance=(before.subsecond+1024u-target)%1024u;
    if (!tp_f030_rtc_read(&after,8) ||
        !tp_f030_rtc_elapsed(before,after,1023u,&elapsed) || elapsed>=distance || distance-elapsed<2u ||
        (RTC->ISR & RTC_ISR_ALRAF) || (EXTI->PR & ALARM_LINE)) {
        tp_f030_rtc_disarm(); return false;
    }
    *started=before;
    return true;
}
