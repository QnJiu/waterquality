#include "FreeRTOS.h"
#include "task.h"
#include "stm32f4xx.h"
#include "tp_f407_board_config.h"
#include "tp_f407_emergency.h"
#ifdef TP_F407_DIAGNOSTICS
#include "tp_f407_diagnostics.h"
#endif

/* Inspect in a debugger after a failed start. */
volatile tp_result tp_startup_result = TP_BUSY;
static tp_f407_app app;
static tp_f407_app_options options;

static void halt(void) {
    __disable_irq();
    tp_f407_emergency_off();
    for (;;) { __NOP(); }
}
void tp_platform_assert(const char *file, int line) {
    (void)file; (void)line; halt();
}
void vApplicationMallocFailedHook(void) { tp_platform_assert(__FILE__,__LINE__); }
void vApplicationStackOverflowHook(TaskHandle_t task, char *name) {
    (void)task; (void)name; tp_platform_assert(__FILE__,__LINE__);
}
int main(void) {
    SystemCoreClockUpdate();
    tp_startup_result=tp_f407_board_options(&options);
    if(tp_startup_result!=TP_OK) halt();
    tp_startup_result=tp_f407_app_init(&app,&options);
    if(tp_startup_result!=TP_OK) halt();
#ifdef TP_F407_DIAGNOSTICS
    tp_startup_result=tp_f407_diagnostics_start(&app);
    if(tp_startup_result!=TP_OK) halt();
#endif
    vTaskStartScheduler();
    halt();
}
