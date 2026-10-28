#ifndef TP_CPU_USAGE_H
#define TP_CPU_USAGE_H
#include <stdint.h>

/* Caller serializes access and reads the 32-bit timer at least once per wrap. */
typedef struct { uint32_t previous; uint64_t high; } tp_counter_extension;
static inline uint64_t tp_counter_extend(tp_counter_extension *counter,uint32_t now) {
    if (now<counter->previous) counter->high+=UINT64_C(1)<<32;
    counter->previous=now;
    return counter->high | now;
}
/* UINT32_MAX means the interval is not usable. */
static inline uint32_t tp_cpu_busy_percent(uint64_t elapsed,uint64_t idle) {
    if (!elapsed || idle>elapsed || elapsed>UINT64_MAX/100u) return UINT32_MAX;
    return (uint32_t)((elapsed-idle)*100u/elapsed);
}
#endif
