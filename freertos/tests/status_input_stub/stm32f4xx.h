#include <stdint.h>
typedef struct { uint32_t AHB1ENR; } RCC_TypeDef;
typedef struct { uint32_t MODER,PUPDR,IDR; } GPIO_TypeDef;
extern RCC_TypeDef fake_rcc; extern GPIO_TypeDef fake_gpio;
#define RCC (&fake_rcc)
#define GPIOC (&fake_gpio)
#define RCC_AHB1ENR_GPIOCEN (1u<<2)
