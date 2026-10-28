#ifndef STM32F4XX_H
#define STM32F4XX_H
#include <stdint.h>
typedef struct { uint32_t CTRL,LOAD,VAL; } fake_systick;
typedef struct { uint32_t ICER[3],ICPR[3]; } fake_nvic;
typedef struct { uint32_t ICSR,VTOR; } fake_scb;
extern fake_systick test_systick; extern fake_nvic test_nvic; extern fake_scb test_scb;
extern uint32_t test_ipsr,test_control,test_faultmask,test_irq_disabled,test_dsb,test_isb;
#define SysTick (&test_systick)
#define NVIC (&test_nvic)
#define SCB (&test_scb)
#define SCB_ICSR_PENDSTCLR_Msk (1u<<25)
#define SCB_ICSR_PENDSVCLR_Msk (1u<<27)
#define __get_IPSR() (test_ipsr)
#define __get_CONTROL() (test_control)
#define __get_FAULTMASK() (test_faultmask)
#define __disable_irq() (test_irq_disabled++)
#define __DSB() (test_dsb++)
#define __ISB() (test_isb++)
#endif
