/* Runs before C runtime initialization. The linker binds g_pfnVectors to the
 * selected image slot; do not depend on initialized globals in this function. */
#include "stm32f4xx.h"
extern uint32_t g_pfnVectors[];
void tp_vendor_system_init(void);
void SystemInit(void) {
    tp_vendor_system_init();
    SCB->VTOR=(uint32_t)(uintptr_t)g_pfnVectors;
    __DSB(); __ISB();
}
