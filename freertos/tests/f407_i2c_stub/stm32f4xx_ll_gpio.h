#ifndef FAKE_GPIO_H
#define FAKE_GPIO_H
#include "stm32f4xx.h"
#define LL_GPIO_PIN_8 (1u<<8)
#define LL_GPIO_PIN_9 (1u<<9)
#define LL_GPIO_PIN_10 (1u<<10)
#define LL_GPIO_PIN_11 (1u<<11)
#define LL_GPIO_AF_4 4u
#define LL_GPIO_OUTPUT_OPENDRAIN 1u
#define LL_GPIO_SPEED_FREQ_HIGH 2u
#define LL_GPIO_PULL_NO 0u
#define LL_GPIO_MODE_ALTERNATE 2u
#define LL_GPIO_MODE_INPUT 0u
static inline void LL_GPIO_SetAFPin_8_15(GPIO_TypeDef *g,uint32_t p,uint32_t v) { (void)g;(void)p;(void)v; }
static inline void LL_GPIO_SetPinOutputType(GPIO_TypeDef *g,uint32_t p,uint32_t v) { (void)g;(void)p;(void)v; }
static inline void LL_GPIO_SetPinSpeed(GPIO_TypeDef *g,uint32_t p,uint32_t v) { (void)g;(void)p;(void)v; }
static inline void LL_GPIO_SetPinPull(GPIO_TypeDef *g,uint32_t p,uint32_t v) { (void)g;(void)p;(void)v; }
static inline void LL_GPIO_SetPinMode(GPIO_TypeDef *g,uint32_t p,uint32_t v) { if (v) g->modes |= p; else g->modes &= ~p; }
#endif
