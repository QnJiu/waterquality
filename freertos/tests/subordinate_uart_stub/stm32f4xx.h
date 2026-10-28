#ifndef STM32F4XX_H
#define STM32F4XX_H
#include <stdint.h>
typedef struct { uint32_t AHB1ENR, APB1ENR, APB2ENR, CFGR; } RCC_TypeDef;
typedef struct { uint32_t SR,DR,CR1,CR2,CR3,BRR; } USART_TypeDef;
typedef struct { uint32_t BSRR; } GPIO_TypeDef;
typedef struct { uint32_t VTOR; } SCB_Type;
extern SCB_Type fake_scb;
#define SCB (&fake_scb)
extern RCC_TypeDef fake_rcc;
extern USART_TypeDef fake_usart;
extern GPIO_TypeDef fake_gpio;
extern uint32_t SystemCoreClock;
#define RCC (&fake_rcc)
#define UART4 (&fake_usart)
typedef struct { uint32_t CNT; } TIM_TypeDef;
extern TIM_TypeDef fake_tim;
#define TIM5 (&fake_tim)
#define USART2 (&fake_usart)
#define USART6 (&fake_usart)
#define GPIOC (&fake_gpio)
#define GPIOA (&fake_gpio)
#define RCC_AHB1ENR_GPIOAEN 1u
#define RCC_AHB1ENR_GPIOCEN 4u
#define RCC_APB2ENR_USART6EN 32u
#define RCC_CFGR_PPRE2 0xe000u
#define USART6_IRQn 71
#define RCC_APB1ENR_USART2EN 1u
#define RCC_CFGR_SWS 0xcu
#define RCC_CFGR_HPRE 0xf0u
#define RCC_CFGR_PPRE1 0x1c00u
#define USART_SR_PE 1u
#define USART_SR_FE 2u
#define USART_SR_NE 4u
#define USART_SR_ORE 8u
#define USART_SR_RXNE 32u
#define USART_SR_TC 64u
#define USART_SR_TXE 128u
#define USART_CR3_EIE 1u
#define USART_CR1_UE 0x2000u
#define USART_CR1_TE 8u
#define USART_CR1_RE 4u
#define USART_CR1_RXNEIE 32u
#define USART_CR1_PEIE 256u
#define USART2_IRQn 38
#define UART4_IRQn 52
#define RCC_APB1ENR_UART4EN 0x80000u
#define USART_CR1_TXEIE 0x80u
#define USART_CR1_TCIE 0x40u
#define NVIC_SetPriority(irq,prio) ((void)(irq),(void)(prio))
#define NVIC_ClearPendingIRQ(irq) ((void)(irq))
#define NVIC_EnableIRQ(irq) ((void)(irq))
#endif
