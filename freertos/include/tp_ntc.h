#ifndef TP_NTC_H
#define TP_NTC_H
#include "tp_core.h"
/* 10k pull-up to ADC reference, 10k NTC to ground, B=3950 K at 25 C.
 * ADC sample is ratiometric 12 bit; rejects open/short and temperatures outside
 * -20..150 C. Component tolerances and calibration are external concerns. */
tp_result tp_ntc_10k_3950_celsius(uint16_t raw,float *temperature_c);
#endif
