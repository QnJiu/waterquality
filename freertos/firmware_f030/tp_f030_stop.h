#ifndef TP_F030_STOP_H
#define TP_F030_STOP_H
#include <stdbool.h>
/* Before scheduler startup; owns RTC Alarm A, EXTI17 and RTC IRQ. */
bool tp_f030_stop_init(void);
#endif
