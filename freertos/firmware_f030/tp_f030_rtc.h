#ifndef TP_F030_RTC_H
#define TP_F030_RTC_H
#include "tp_f030_rtc_time.h"
#include <stdbool.h>
#include <stdint.h>
/* Board initializes LSE, RTC calendar and 31/1023 prescalers; RTC must be in
 * BYPSHAD mode with backup writes enabled. Alarm A and EXTI17 are exclusive. */
bool tp_f030_rtc_ready(void);
/* Optional reference startup. Exclusive RTC ownership is required. Does not
 * reset the backup domain; existing initialized calendars are left untouched.
 * poll_limit is a CPU-loop bound, not a duration. Enable interrupts and configure
 * EXTI17/NVIC separately before using the alarm. */
bool tp_f030_rtc_init(uint32_t poll_limit);
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
/* With interrupts masked and Alarm A owned exclusively: disable the alarm,
 * wait for hardware, clear ALRAF and finally EXTI17. On timeout leave pending
 * flags for the caller; do not enter sleep. Does not clear NVIC pending state. */
bool tp_f030_rtc_ack_alarm(uint32_t poll_limit);
#endif
