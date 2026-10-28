#ifndef TP_F407_IWDG_H
#define TP_F407_IWDG_H
#include "tp_core.h"
/* Starts the irreversible watchdog after configuring PR=256, RLR=1250.
 * A running watchdog cannot be stopped without reset. The loop bound assumes
 * the reference 16 MHz core clock; on error, reset or wait for watchdog reset. */
tp_result tp_f407_iwdg_start(void);
/* Nonblocking; used only by the health supervisor. */
void tp_f407_iwdg_feed(void);
#endif
