#include "tp_ntc.h"
#include <math.h>
tp_result tp_ntc_10k_3950_celsius(uint16_t raw,float *out) {
    if (!out || raw<20u || raw>4075u) return TP_INVALID;
    float ratio=(float)raw/(4095.0f-(float)raw);
    float kelvin=1.0f/(1.0f/298.15f+logf(ratio)/3950.0f);
    float celsius=kelvin-273.15f;
    if (!isfinite(celsius) || celsius < -20.0f || celsius > 150.0f) return TP_INVALID;
    *out=celsius; return TP_OK;
}
