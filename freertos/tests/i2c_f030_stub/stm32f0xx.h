#ifndef TEST_I2C_F030_STM32_H
#define TEST_I2C_F030_STM32_H
#include <stdint.h>
typedef struct { uint32_t CFGR3, AHBENR, APB1ENR; } RCC_TypeDef;
typedef struct { uint32_t OTYPER, PUPDR, AFR[2], MODER; } GPIO_TypeDef;
typedef struct { uint32_t CR1, CR2, TIMINGR, ISR, ICR, TXDR, RXDR; } I2C_TypeDef;
extern RCC_TypeDef fake_rcc;
extern GPIO_TypeDef fake_gpio_b;
extern I2C_TypeDef fake_i2c;
extern uint32_t SystemCoreClock;
#define RCC (&fake_rcc)
#define GPIOB (&fake_gpio_b)
#define I2C1 (&fake_i2c)
#define RCC_CFGR3_I2C1SW (1u<<4)
#define RCC_AHBENR_GPIOBEN (1u<<18)
#define RCC_APB1ENR_I2C1EN (1u<<21)
#define I2C_CR1_PE 1u
#define I2C_CR2_SADD 0x3ffu
#define I2C_CR2_RD_WRN (1u<<10)
#define I2C_CR2_START (1u<<13)
#define I2C_CR2_STOP (1u<<14)
#define I2C_CR2_NBYTES_Pos 16u
#define I2C_CR2_RELOAD (1u<<24)
#define I2C_CR2_AUTOEND (1u<<25)
#define I2C_ISR_TXIS (1u<<1)
#define I2C_ISR_RXNE (1u<<2)
#define I2C_ISR_NACKF (1u<<4)
#define I2C_ISR_STOPF (1u<<5)
#define I2C_ISR_TCR (1u<<7)
#define I2C_ISR_BERR (1u<<8)
#define I2C_ISR_ARLO (1u<<9)
#define I2C_ISR_OVR (1u<<10)
#define I2C_ISR_BUSY (1u<<15)
#define I2C_ICR_NACKCF (1u<<4)
#define I2C_ICR_STOPCF (1u<<5)
#define I2C_ICR_BERRCF (1u<<8)
#define I2C_ICR_ARLOCF (1u<<9)
#define I2C_ICR_OVRCF (1u<<10)
#endif
