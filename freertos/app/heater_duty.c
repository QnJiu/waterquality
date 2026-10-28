#include "tp_heater_duty.h"
#include <math.h>
tp_result tp_heater_duty_ticks(float fraction,uint32_t period,uint32_t *ticks) {
    if (!ticks || !period || period>1000000u || !isfinite(fraction) || fraction<0.0f || fraction>1.0f) return TP_INVALID;
    *ticks=(uint32_t)(fraction*(float)period+0.5f);
    return TP_OK;
}
