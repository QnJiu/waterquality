#include "tp_f030_rtc.h"
#include "stm32f0xx.h"
#include <stddef.h>

#define ALARM_LINE EXTI_PR_PR17
#define ALL_FIELDS (RTC_ALRMAR_MSK1|RTC_ALRMAR_MSK2|RTC_ALRMAR_MSK3|RTC_ALRMAR_MSK4)
#define PRESCALERS ((31u<<16)|1023u)
#ifndef TP_F030_RTC_WAIT_HOOK
#define TP_F030_RTC_WAIT_HOOK() ((void)0)
#endif
#ifndef TP_F030_RTC_CLEAR_ALARM
#define TP_F030_RTC_CLEAR_ALARM() \
    (RTC->ISR = (RTC->ISR | RTC_ISR_TAMP2F | RTC_ISR_TAMP1F | RTC_ISR_TSOVF | \
                 RTC_ISR_TSF | RTC_ISR_RSF) & ~RTC_ISR_ALRAF)
#endif
#ifndef TP_F030_RTC_CLEAR_EXTI
#define TP_F030_RTC_CLEAR_EXTI() (EXTI->PR = ALARM_LINE)
#endif
static void unlock(void) { RTC->WPR=0xcau; RTC->WPR=0x53u; }
static void lock(void) { RTC->WPR=0xffu; }
bool tp_f030_rtc_ready(void) {
    return (RCC->BDCR & (RCC_BDCR_RTCEN|RCC_BDCR_RTCSEL|RCC_BDCR_LSERDY))==
        (RCC_BDCR_RTCEN|RCC_BDCR_RTCSEL_LSE|RCC_BDCR_LSERDY) &&
        (PWR->CR & PWR_CR_DBP) && (RTC->ISR & RTC_ISR_INITS) &&
        (RTC->CR & RTC_CR_BYPSHAD) && !(RTC->CR & RTC_CR_FMT) && (RTC->PRER & (RTC_PRER_PREDIV_A|RTC_PRER_PREDIV_S))==PRESCALERS;
}
/* This reference initialization owns the RTC and its backup domain. */
bool tp_f030_rtc_init(uint32_t poll_limit) {
    if (!poll_limit) return false;
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_DBP;
    uint32_t n=poll_limit;
    while (!(PWR->CR & PWR_CR_DBP)) {
        TP_F030_RTC_WAIT_HOOK();
        if (!--n) return false;
    }
    uint32_t source=RCC->BDCR & RCC_BDCR_RTCSEL;
    if (source && source!=RCC_BDCR_RTCSEL_LSE) return false;
    RCC->BDCR |= RCC_BDCR_LSEON;
    n=poll_limit;
    while (!(RCC->BDCR & RCC_BDCR_LSERDY)) {
        TP_F030_RTC_WAIT_HOOK();
        if (!--n) return false;
    }
    RCC->BDCR |= RCC_BDCR_RTCSEL_LSE | RCC_BDCR_RTCEN;
    /* Preserve an existing calendar and reject an incompatible configuration. */
    if (RTC->ISR & RTC_ISR_INITS) return tp_f030_rtc_ready();
    if ((RTC->CR & (RTC_CR_ALRAE|RTC_CR_ALRAIE)) ||
        (RTC->ISR & RTC_ISR_ALRAF) || (EXTI->PR & ALARM_LINE)) return false;
    unlock();
    RTC->ISR |= RTC_ISR_INIT;
    n=poll_limit;
    while (!(RTC->ISR & RTC_ISR_INITF)) {
        TP_F030_RTC_WAIT_HOOK();
        if (!--n) { RTC->ISR &= ~RTC_ISR_INIT; lock(); return false; }
    }
    /* PRER requires two writes, synchronous divider first. */
    RTC->PRER=1023u;
    RTC->PRER=PRESCALERS;
    RTC->TR=0;
    RTC->DR=(1u<<16)|(1u<<13)|(1u<<8)|1u; /* 2001-01-01, Monday */
    RTC->CR=(RTC->CR & ~RTC_CR_FMT)|RTC_CR_BYPSHAD;
    RTC->ISR &= ~RTC_ISR_INIT;
    lock();
    n=poll_limit;
    while (!(RTC->ISR & RTC_ISR_INITS)) {
        TP_F030_RTC_WAIT_HOOK();
        if (!--n) return false;
    }
    return tp_f030_rtc_ready();
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
bool tp_f030_rtc_ack_alarm(uint32_t poll_limit) {
    if (!poll_limit || !tp_f030_rtc_ready()) return false;
    tp_f030_rtc_disarm();
    uint32_t n=poll_limit;
    while (!(RTC->ISR & RTC_ISR_ALRAWF)) {
        TP_F030_RTC_WAIT_HOOK();
        if (!--n) return false;
    }
    /* ALRAF is write-zero-to-clear; preserve other RTC event flags. */
    TP_F030_RTC_CLEAR_ALARM();
    n=poll_limit;
    while (RTC->ISR & RTC_ISR_ALRAF) {
        TP_F030_RTC_WAIT_HOOK();
        if (!--n) return false;
    }
    /* EXTI pending is write-one-to-clear; never write back other lines. */
    TP_F030_RTC_CLEAR_EXTI();
    return true;
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
