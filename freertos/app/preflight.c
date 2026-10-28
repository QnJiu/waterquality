#include "tp_preflight.h"
#include <math.h>
tp_result tp_preflight_check(const tp_preflight_io *io,uint16_t *faults) {
    if(!io || !faults || !io->interlocks_ok || !io->temperature || !io->resource_faults ||
       !isfinite(io->max_temperature_c)) return TP_INVALID;
    uint16_t f=io->interlocks_ok(io->ctx) ? 0 : TP_PREFLIGHT_INTERLOCK;
    float temp=0;
    if(io->temperature(io->ctx,&temp)!=TP_OK || !isfinite(temp)) f|=TP_PREFLIGHT_TEMPERATURE_SENSOR;
    else if(temp>=io->max_temperature_c) f|=TP_PREFLIGHT_OVERTEMPERATURE;
    uint16_t resources=io->resource_faults(io->ctx);
    if(resources & ~(TP_PREFLIGHT_REAGENT|TP_PREFLIGHT_LEVEL|TP_PREFLIGHT_RESOURCE_UNKNOWN))
        resources=TP_PREFLIGHT_RESOURCE_UNKNOWN;
    *faults=f|resources;
    return *faults ? TP_IO_ERROR : TP_OK;
}
