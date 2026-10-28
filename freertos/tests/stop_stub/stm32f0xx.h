#ifndef TEST_STOP_STM32_H
#define TEST_STOP_STM32_H
#include <stdint.h>
typedef struct { uint32_t CR,ISR; } RTC_TypeDef;
typedef struct { uint32_t IMR,RTSR,FTSR,PR; } EXTI_TypeDef;
typedef struct { uint32_t ISER[1], ISPR[1]; } NVIC_Type;
typedef struct { uint32_t ICSR,SCR; } SCB_Type;
typedef struct { uint32_t CTRL,LOAD,VAL; } SysTick_Type;
typedef struct { uint32_t CR1; } TIM_TypeDef;
typedef struct { uint32_t CR; } PWR_TypeDef;
extern RTC_TypeDef rtc; extern EXTI_TypeDef exti; extern NVIC_Type nvic;
extern SCB_Type scb; extern SysTick_Type systick; extern TIM_TypeDef tim;
extern PWR_TypeDef pwr; extern uint32_t SystemCoreClock;
#define RTC (&rtc)
#define EXTI (&exti)
#define NVIC (&nvic)
#define SCB (&scb)
#define SysTick (&systick)
#define TIM3 (&tim)
#define PWR (&pwr)
#define RTC_CR_ALRAE 1u
#define RTC_CR_ALRAIE 2u
#define RTC_CR_TSIE 8u
#define RTC_ISR_ALRAF 4u
#define SCB_ICSR_PENDSTSET_Msk (1u<<26)
#define SCB_ICSR_PENDSVSET_Msk (1u<<28)
#define SCB_SCR_SLEEPDEEP_Msk 4u
#define SysTick_CTRL_CLKSOURCE_Msk 4u
#define SysTick_CTRL_TICKINT_Msk 2u
#define SysTick_CTRL_ENABLE_Msk 1u
#define TIM_CR1_CEN 1u
#define PWR_CR_PDDS 2u
#define PWR_CR_LPDS 1u
#define RTC_IRQn 2
void fake_disable(void); void fake_enable(void); void fake_wfi(void);
#define __disable_irq() fake_disable()
#define __enable_irq() fake_enable()
#define __DSB() ((void)0)
#define __ISB() ((void)0)
#define __WFI() fake_wfi()
static inline uint32_t NVIC_GetPendingIRQ(int n) { return (nvic.ISPR[0]>>n)&1u; }
static inline void NVIC_ClearPendingIRQ(int n) { nvic.ISPR[0]&=~(1u<<n); }
static inline void NVIC_SetPriority(int n,unsigned p) { (void)n;(void)p; }
static inline void NVIC_EnableIRQ(int n) { nvic.ISER[0]|=1u<<n; }
void tp_platform_assert(const char *,int);
#endif
