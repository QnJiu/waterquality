#ifndef TEST_STM32F0XX_H
#define TEST_STM32F0XX_H
#include <stdint.h>
typedef struct { volatile uint32_t BDCR, APB1ENR; } RCC_TypeDef;
typedef struct { volatile uint32_t CR; } PWR_TypeDef;
typedef struct { volatile uint32_t TR, DR, CR, ISR, PRER, ALRMAR, WPR, SSR, ALRMASSR; } RTC_TypeDef;
typedef struct { volatile uint32_t IMR, RTSR, PR; } EXTI_TypeDef;
extern RCC_TypeDef rcc; extern PWR_TypeDef pwr; extern RTC_TypeDef rtc; extern EXTI_TypeDef exti;
#define RCC (&rcc)
#define PWR (&pwr)
#define RTC (&rtc)
#define EXTI (&exti)
#define RCC_APB1ENR_PWREN (1u<<28)
#define RCC_BDCR_LSEON (1u<<0)
#define RTC_ISR_INIT (1u<<7)
#define RTC_ISR_INITF (1u<<6)
#define RTC_CR_FMT (1u<<6)
void rtc_test_wait_hook(void);
#define TP_F030_RTC_WAIT_HOOK() rtc_test_wait_hook()
#define RCC_BDCR_RTCEN (1u<<15)
#define RCC_BDCR_RTCSEL (3u<<8)
#define RCC_BDCR_RTCSEL_LSE (1u<<8)
#define RCC_BDCR_LSERDY (1u<<1)
#define PWR_CR_DBP (1u<<8)
#define RTC_ISR_INITS (1u<<4)
#define RTC_ISR_ALRAF (1u<<8)
#define RTC_ISR_ALRAWF (1u<<0)
#define RTC_CR_BYPSHAD (1u<<5)
#define RTC_CR_ALRAE (1u<<8)
#define RTC_CR_ALRAIE (1u<<12)
#define RTC_PRER_PREDIV_A (127u<<16)
#define RTC_PRER_PREDIV_S 32767u
#define RTC_ALRMAR_MSK1 (1u<<7)
#define RTC_ALRMAR_MSK2 (1u<<15)
#define RTC_ALRMAR_MSK3 (1u<<23)
#define RTC_ALRMAR_MSK4 (1u<<31)
#define RTC_ALRMASSR_MASKSS_Pos 24u
#define EXTI_PR_PR17 (1u<<17)
#endif
