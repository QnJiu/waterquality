#ifndef TEST_STM32F0_H
#define TEST_STM32F0_H
#include <stdint.h>
typedef struct { uint32_t AHBENR,APB1ENR,APB2ENR,CFGR,CFGR3; } RCC_TypeDef;
typedef struct { uint32_t AFR[2],PUPDR,MODER,IDR,BSRR,OTYPER,OSPEEDR; } GPIO_TypeDef;
typedef struct { uint32_t EXTICR[4]; } SYSCFG_TypeDef;
typedef struct { uint32_t IMR,RTSR,FTSR,PR; } EXTI_TypeDef;
typedef struct { uint32_t CR1,PSC,ARR,EGR,SR,DIER,CNT; } TIM_TypeDef;
typedef struct { uint32_t ISR,ICR,RDR,TDR,CR1,CR2,CR3,BRR; } USART_TypeDef;
extern RCC_TypeDef fake_rcc;
extern GPIO_TypeDef fake_gpio;
extern GPIO_TypeDef fake_gpio_b;
extern SYSCFG_TypeDef fake_syscfg;
extern EXTI_TypeDef fake_exti;
extern TIM_TypeDef fake_tim;
extern USART_TypeDef fake_usart;
extern uint32_t SystemCoreClock;
#define RCC (&fake_rcc)
#define GPIOA (&fake_gpio)
#define GPIOB (&fake_gpio_b)
#define SYSCFG (&fake_syscfg)
#define EXTI (&fake_exti)
#define TIM3 (&fake_tim)
#define USART1 (&fake_usart)
#define USART_ISR_PE (1u<<0)
#define USART_ISR_FE (1u<<1)
#define USART_ISR_NE (1u<<2)
#define USART_ISR_ORE (1u<<3)
#define USART_ISR_RXNE (1u<<5)
#define USART_ISR_TC (1u<<6)
#define USART_ISR_TXE (1u<<7)
#define USART_ICR_PECF (1u<<0)
#define USART_ICR_FECF (1u<<1)
#define USART_ICR_NCF (1u<<2)
#define USART_ICR_ORECF (1u<<3)
#define USART_ICR_TCCF (1u<<6)
#define USART_CR1_UE (1u<<0)
#define USART_CR1_RE (1u<<2)
#define USART_CR1_TE (1u<<3)
#define USART_CR1_RXNEIE (1u<<5)
#define USART_CR1_TCIE (1u<<6)
#define USART_CR1_TXEIE (1u<<7)
#define USART_CR1_PEIE (1u<<8)
#define USART_CR3_EIE 1u
#define TIM_SR_UIF 1u
#define TIM_EGR_UG 1u
#define TIM_DIER_UIE 1u
#define TIM_CR1_CEN 1u
#define RCC_AHBENR_GPIOAEN (1u<<17)
#define RCC_AHBENR_GPIOBEN (1u<<18)
#define RCC_APB2ENR_SYSCFGEN 1u
#define SYSCFG_EXTICR1_EXTI2_PB (1u<<8)
#define RCC_APB1ENR_TIM3EN (1u<<1)
#define RCC_APB2ENR_USART1EN (1u<<14)
#define RCC_CFGR_SWS (3u<<2)
#define RCC_CFGR_HPRE (15u<<4)
#define RCC_CFGR_PPRE (7u<<8)
#define RCC_CFGR3_USART1SW 3u
#define TIM3_IRQn 16
#define USART1_IRQn 27
#define EXTI0_1_IRQn 5
#define EXTI2_3_IRQn 6
static inline void NVIC_SetPriority(int irq,unsigned p) { (void)irq; (void)p; }
static inline void NVIC_ClearPendingIRQ(int irq) { (void)irq; }
static inline void NVIC_EnableIRQ(int irq) { (void)irq; }
#endif
