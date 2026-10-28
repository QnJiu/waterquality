#include "tp_f407_boot_board.h"
#include "tp_f407_handoff.h"
#include "tp_f407_emergency.h"
#include "stm32f4xx.h"
/* Debugger-visible result; failures enter a terminal safe path. */
volatile tp_result tp_boot_startup_result=TP_BUSY;
static tp_f407_boot boot;
static tp_f407_boot_options options;
static tp_boot_target target;
static void halt(void) { __disable_irq(); tp_f407_emergency_off(); for(;;) __NOP(); }
int main(void) {
    tp_boot_startup_result=tp_f407_boot_board_options(&options);
    if(tp_boot_startup_result!=TP_OK) halt();
    tp_boot_startup_result=tp_f407_boot_init(&boot,&options);
    if(tp_boot_startup_result!=TP_OK) halt();
    tp_boot_startup_result=tp_f407_boot_prepare(&boot,&target);
    if(tp_boot_startup_result!=TP_OK) halt();
    tp_boot_startup_result=tp_f407_boot_board_quiesce();
    if(tp_boot_startup_result!=TP_OK) halt();
    tp_boot_startup_result=tp_f407_handoff(&target);
    halt();
}
