#include "tp_f407_recipe.h"
#include "tp_adc_reader.h"
#include "tp_pulse_counter.h"

tp_result tp_f407_recipe_validate(const tp_controller *c,const tp_step *steps,size_t count,
                                   const tp_valve_policy *valves,size_t adc_channels) {
    if (!valves || tp_valve_policy_validate(valves)!=TP_OK || !adc_channels ||
        adc_channels>TP_ADC_MAX_CHANNELS || tp_validate_recipe(c,steps,count)!=TP_OK)
        return TP_INVALID;
    unsigned readings=0;
    for (size_t i=0;i<count;i++) {
        const tp_step *s=&steps[i];
        if (s->kind==TP_STEP_VALVES && !tp_valve_policy_allows(valves,s->arg.valves.mask))
            return TP_INVALID;
        if (s->kind==TP_STEP_PUMP) {
            tp_pulse_counter counter={0};
            if (tp_pulse_begin(&counter,s->arg.pump.steps,s->arg.pump.frequency_hz)!=TP_OK)
                return TP_INVALID;
        }
        if (s->kind==TP_STEP_READ) {
            if (s->arg.read.channel>=adc_channels) return TP_INVALID;
            readings |= 1u << s->arg.read.slot;
        }
        if (s->kind==TP_STEP_CALCULATE && readings!=(1u<<TP_READ_COUNT)-1u)
            return TP_INVALID;
    }
    return TP_OK;
}
