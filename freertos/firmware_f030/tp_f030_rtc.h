#ifndef TP_F030_RTC_H
#define TP_F030_RTC_H
#include "tp_f030_rtc_time.h"
#include <stdbool.h>
#include <stdint.h>
/* Board initializes LSE, RTC calendar and 31/1023 prescalers; RTC must be in
 * BYPSHAD mode with backup writes enabled. Alarm A and EXTI17 are exclusive. */
bool tp_f030_rtc_ready(void);
/* Retry bounded direct-register reads to avoid a rollover-torn timestamp. */
bool tp_f030_rtc_read(tp_f030_rtc_stamp *stamp, uint32_t attempts);
/* Called with interrupts masked. Caller enables EXTI17/NVIC first and checks
 * all other pending wake conditions before WFI. False means do not sleep.
 * Existing pending alarm events are never cleared by this function. */
bool tp_f030_rtc_arm(uint32_t budget_ms,uint32_t poll_limit,
                      tp_f030_rtc_stamp *started);
/* Called with interrupts masked; preserves pending alarm and EXTI flags for the
 * caller's interrupt/cleanup sequence. */
void tp_f030_rtc_disarm(void);
#endif
