#ifndef TP_F407_DIAGNOSTICS_H
#define TP_F407_DIAGNOSTICS_H
#include "tp_f407_app.h"

/* Stack values are minimum unused words since task creation; UINT32_MAX means
 * the optional task is absent. Heap values are bytes. CPU is 0-100 percent;
 * UINT32_MAX means insufficient or invalid samples. Read after updates > 0. */
typedef struct {
    uint32_t updates, heap_free_bytes, heap_min_bytes, cpu_busy_percent;
    uint32_t control_words, storage_words, temperature_words, clock_words;
    uint32_t uart_words, uart1_words, health_words, events_words, diagnostics_words;
} tp_f407_diagnostics;
extern volatile tp_f407_diagnostics tp_debug_resources;
/* Call once before starting the scheduler, after successful app initialization. */
tp_result tp_f407_diagnostics_start(tp_f407_app *app);
#endif
