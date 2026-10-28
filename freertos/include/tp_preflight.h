#ifndef TP_PREFLIGHT_H
#define TP_PREFLIGHT_H
#include "tp_core.h"
#define TP_PREFLIGHT_INTERLOCK 0x0001u
#define TP_PREFLIGHT_TEMPERATURE_SENSOR 0x0002u
#define TP_PREFLIGHT_OVERTEMPERATURE 0x0004u
#define TP_PREFLIGHT_REAGENT 0x0008u
#define TP_PREFLIGHT_LEVEL 0x0010u
#define TP_PREFLIGHT_RESOURCE_UNKNOWN 0x0020u
#define TP_PREFLIGHT_NOT_RUN 0x8000u
/* All callbacks are nonblocking and return a fresh sample. The resource
 * callback reports reagent/level failures or unknown status in the bits above. */
typedef struct {
    void *ctx;
    bool (*interlocks_ok)(void *);
    tp_result (*temperature)(void *,float *);
    uint16_t (*resource_faults)(void *);
    float max_temperature_c;
} tp_preflight_io;
tp_result tp_preflight_check(const tp_preflight_io *,uint16_t *faults);
#endif
