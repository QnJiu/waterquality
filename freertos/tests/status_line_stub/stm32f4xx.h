#ifndef STATUS_LINE_STM32_H
#define STATUS_LINE_STM32_H
#include <stdint.h>
typedef struct { uint32_t AHBENR, AHB1ENR; } RCC_TypeDef;
typedef struct { uint32_t MODER,PUPDR,IDR,BSRR,OTYPER,OSPEEDR; } GPIO_TypeDef;
extern RCC_TypeDef fake_rcc; extern GPIO_TypeDef fake_gpio_b, fake_gpio_c;
#define RCC (&fake_rcc)
#define GPIOB (&fake_gpio_b)
#define GPIOC (&fake_gpio_c)
#define RCC_AHBENR_GPIOBEN (1u<<18)
#define RCC_AHB1ENR_GPIOCEN (1u<<2)
#endif
