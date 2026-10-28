#ifndef TP_F407_HANDOFF_H
#define TP_F407_HANDOFF_H
#include "tp_boot_target.h"
/* Takes a previously prepared and authenticated target while its image is
 * immutable. Call once in privileged thread mode before starting an RTOS.
 * Caller must stop all DMA, peripheral interrupt sources and timers, ensure
 * no pending NMI/fault, return clocks/peripherals to the application's expected
 * startup state, and leave output hardware safe. IWDG cannot be stopped.
 * Success does not return. On invalid preconditions returns TP_INVALID without
 * changing processor or interrupt state. Not an image authentication API. */
tp_result tp_f407_handoff(const tp_boot_target *target);
#endif
