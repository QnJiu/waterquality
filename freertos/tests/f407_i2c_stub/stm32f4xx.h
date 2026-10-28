#ifndef FAKE_STM32_H
#define FAKE_STM32_H
#include <stdint.h>
typedef struct { uint32_t CR1, CR2, SR1, SR2, DR, CCR, TRISE; } I2C_TypeDef;
typedef struct { uint32_t AHB1ENR, APB1ENR, APB1RSTR, CFGR; } RCC_TypeDef;
typedef struct { uint32_t DEMCR; } CoreDebug_Type;
typedef struct { uint32_t CTRL, CYCCNT; } DWT_Type;
typedef struct { uint32_t modes; } GPIO_TypeDef;
extern I2C_TypeDef i2c2_fake, i2c3_fake;
extern RCC_TypeDef rcc_fake;
extern CoreDebug_Type debug_fake;
extern DWT_Type dwt_fake;
extern GPIO_TypeDef gpioa_fake,gpiob_fake,gpioc_fake;
extern uint32_t SystemCoreClock;
#define I2C2 (&i2c2_fake)
#define I2C3 (&i2c3_fake)
#define RCC (&rcc_fake)
#define CoreDebug (&debug_fake)
#define DWT (&dwt_fake)
#define GPIOA (&gpioa_fake)
#define GPIOB (&gpiob_fake)
#define GPIOC (&gpioc_fake)
#define I2C_SR1_BERR (1u<<8)
#define I2C_SR1_ARLO (1u<<9)
#define I2C_SR1_AF (1u<<10)
#define I2C_SR1_OVR (1u<<11)
#define I2C_SR1_TIMEOUT (1u<<14)
#define I2C_SR1_SB (1u<<0)
#define I2C_SR1_ADDR (1u<<1)
#define I2C_SR1_BTF (1u<<2)
#define I2C_SR1_RXNE (1u<<6)
#define I2C_SR1_TXE (1u<<7)
#define I2C_SR2_BUSY (1u<<1)
#define I2C_CR1_PE (1u<<0)
#define I2C_CR1_START (1u<<8)
#define I2C_CR1_STOP (1u<<9)
#define I2C_CR1_ACK (1u<<10)
#define RCC_APB1RSTR_I2C2RST (1u<<22)
#define RCC_APB1RSTR_I2C3RST (1u<<23)
#define RCC_APB1ENR_I2C2EN (1u<<22)
#define RCC_APB1ENR_I2C3EN (1u<<23)
#define RCC_AHB1ENR_GPIOAEN (1u<<0)
#define RCC_AHB1ENR_GPIOBEN (1u<<1)
#define RCC_AHB1ENR_GPIOCEN (1u<<2)
#define RCC_CFGR_SWS 12u
#define RCC_CFGR_HPRE 240u
#define RCC_CFGR_PPRE1 7168u
#define CoreDebug_DEMCR_TRCENA_Msk (1u<<24)
#define DWT_CTRL_CYCCNTENA_Msk 1u
static inline uint32_t __get_PRIMASK(void) { return 0; }
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t x) { (void)x; }
#endif
