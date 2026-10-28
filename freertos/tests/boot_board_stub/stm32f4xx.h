#ifndef STM32F4XX_H
#define STM32F4XX_H
void tp_test_disable_irq(void);
void SystemCoreClockUpdate(void);
#define __disable_irq() tp_test_disable_irq()
#endif
