#ifndef TP_F407_RECIPE_H
#define TP_F407_RECIPE_H
#include "tp_core.h"
#include "tp_valve_policy.h"

/* Check the generic recipe and the limits of the reference F407 drivers.
 * Does not verify the fluidic method or the actual board wiring. */
tp_result tp_f407_recipe_validate(const tp_controller *,const tp_step *,size_t,
                                   const tp_valve_policy *,size_t adc_channels);
#endif
