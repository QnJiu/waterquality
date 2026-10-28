#ifndef TP_F407_TIMEBASE_H
#define TP_F407_TIMEBASE_H
#include "tp_core.h"

/* Initialize once before the scheduler; later calls do not reset TIM5. */
tp_result tp_f407_timebase_init(void);
#endif
