#include "tp_f030_subordinate.h"
#include "tp_f030_board_inputs.h"
#include "stm32f0xx.h"

static tp_f030_subordinate subordinate;
volatile tp_result tp_f030_startup_result=TP_BUSY;

void tp_platform_assert(const char *file,int line) {
    (void)file; (void)line;
    __disable_irq();
    for(;;) { __NOP(); }
}
void vApplicationMallocFailedHook(void) { tp_platform_assert(__FILE__,__LINE__); }
void vApplicationStackOverflowHook(TaskHandle_t task,char *name) {
    (void)task; (void)name; tp_platform_assert(__FILE__,__LINE__);
}
int main(void) {
    SystemCoreClockUpdate();
    tp_f030_startup_result=tp_f030_subordinate_init(&subordinate,NULL,tp_f030_board_inputs,
                                                      NULL,false,384,tskIDLE_PRIORITY+2);
    if(tp_f030_startup_result!=TP_OK) tp_platform_assert(__FILE__,__LINE__);
    vTaskStartScheduler();
    tp_platform_assert(__FILE__,__LINE__);
}
